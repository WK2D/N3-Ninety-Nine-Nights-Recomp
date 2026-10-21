import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { pathToFileURL } from 'node:url';

const SECTOR = 2048;
const MAGIC = 'MICROSOFT*XBOX*MEDIA';
const OFFSETS = [0, 0xfb20, 0x20600, 0x2080000, 0xfd90000];

// XDVDFS: volume descriptor at partition sector 32, directory tree links
// in units of four bytes; file extents are relative to the game partition.
export class Disc {
  constructor(filename) {
    this.fd = fs.openSync(filename, 'r');
    this.size = fs.fstatSync(this.fd).size;
    try {
      this.offset = OFFSETS.find(offset => offset + 33 * SECTOR <= this.size &&
        this.read(offset + 32 * SECTOR, 20).toString('ascii') === MAGIC);
      if (this.offset === undefined) throw new Error('No XDVDFS game partition found');
      const volume = this.read(this.offset + 32 * SECTOR, SECTOR);
      if (volume.subarray(2028).toString('ascii') !== MAGIC) throw new Error('Invalid volume footer');
      this.rootSector = volume.readUInt32LE(20);
      this.rootSize = volume.readUInt32LE(24);
    } catch (error) { this.close(); throw error; }
  }

  close() { fs.closeSync(this.fd); }

  read(offset, size) {
    if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(size) ||
        offset < 0 || size < 0 || offset + size > this.size) throw new Error('Extent outside disc');
    const data = Buffer.alloc(size);
    if (fs.readSync(this.fd, data, 0, size, offset) !== size) throw new Error('Short disc read');
    return data;
  }

  entries() {
    const files = [];
    const directories = new Set();
    const names = new Set();
    const pending = [{ sector: this.rootSector, size: this.rootSize, parent: '' }];
    while (pending.length) {
      const { sector, size, parent } = pending.pop();
      if (!size) continue;
      if (size < 14 || size > 32 * 1024 * 1024) throw new Error('Invalid directory size');
      if (directories.has(sector)) throw new Error('Directory cycle or alias');
      directories.add(sector);
      const table = this.read(this.offset + sector * SECTOR, size);
      const nodes = [0];
      const visited = new Set();
      while (nodes.length) {
        const pos = nodes.pop();
        if (visited.has(pos)) throw new Error('Directory node cycle');
        visited.add(pos);
        if (pos + 14 > table.length) throw new Error('Directory node outside table');
        const left = table.readUInt16LE(pos) * 4;
        const right = table.readUInt16LE(pos + 2) * 4;
        const startSector = table.readUInt32LE(pos + 4);
        const length = table.readUInt32LE(pos + 8);
        const attributes = table[pos + 12];
        const nameLength = table[pos + 13];
        if (!nameLength || pos + 14 + nameLength > table.length) throw new Error('Invalid entry name length');
        const name = table.subarray(pos + 14, pos + 14 + nameLength).toString('latin1');
        if (name === '.' || name === '..' || /[<>:"/\\|?*\x00-\x1f]/.test(name) ||
            /[. ]$/.test(name) || /^(con|prn|aux|nul|com[1-9]|lpt[1-9])(\.|$)/i.test(name)) {
          throw new Error(`Unsafe entry name: ${JSON.stringify(name)}`);
        }
        const relative = parent ? `${parent}/${name}` : name;
        if (names.has(relative.toLowerCase())) throw new Error(`Duplicate entry: ${relative}`);
        names.add(relative.toLowerCase());
        const offset = this.offset + startSector * SECTOR;
        if (offset + length > this.size) throw new Error(`Extent outside disc: ${relative}`);
        if (attributes & 0x10) pending.push({ sector: startSector, size: length, parent: relative });
        else files.push({ path: relative, offset, size: length });
        if (left) nodes.push(left);
        if (right) nodes.push(right);
      }
    }
    return files.sort((a, b) => a.path.localeCompare(b.path));
  }

  extract(entry, destination) {
    const root = path.resolve(destination);
    fs.mkdirSync(root, { recursive: true });
    let current = root;
    // Reject links in existing ancestors as well as inside the output tree.
    for (let p = root; ; p = path.dirname(p)) {
      if (fs.lstatSync(p).isSymbolicLink()) throw new Error(`Output ancestor is a link: ${p}`);
      if (path.dirname(p) === p) break;
    }
    const parts = entry.path.split('/');
    for (const part of parts.slice(0, -1)) {
      current = path.join(current, part);
      if (!fs.existsSync(current)) fs.mkdirSync(current);
      if (fs.lstatSync(current).isSymbolicLink() || !fs.statSync(current).isDirectory()) {
        throw new Error(`Unsafe output directory: ${current}`);
      }
    }
    const target = path.resolve(root, ...parts);
    if (!target.startsWith(root + path.sep)) throw new Error('Output escapes destination');
    const fd = fs.openSync(target, 'wx'); // Never overwrite an existing file.
    const hash = crypto.createHash('sha256');
    try {
      for (let done = 0; done < entry.size;) {
        const chunk = this.read(entry.offset + done, Math.min(1024 * 1024, entry.size - done));
        let written = 0;
        while (written < chunk.length) written += fs.writeSync(fd, chunk, written, chunk.length - written);
        hash.update(chunk);
        done += chunk.length;
      }
    } catch (error) { fs.closeSync(fd); fs.unlinkSync(target); throw error; }
    fs.closeSync(fd);
    return hash.digest('hex');
  }
}

function main() {
  const [command, filename, destination, manifestPath] = process.argv.slice(2);
  if (!['list', 'extract'].includes(command) || !filename ||
      (command === 'extract' && (!destination || !manifestPath))) {
    throw new Error('Usage: node tools/disc.mjs list <iso> | extract <iso> <new-output-dir> <manifest.json>');
  }
  const disc = new Disc(filename);
  try {
    const files = disc.entries();
    if (command === 'extract') {
      if (fs.existsSync(destination)) throw new Error('Use a new output directory to avoid mixing disc revisions');
      let count = 0;
      for (const entry of files) {
        entry.sha256 = disc.extract(entry, destination);
        if (++count % 250 === 0 || count === files.length) console.log(`Extracted ${count}/${files.length} files`);
      }
    }
    const manifest = { format: 'XDVDFS', image: path.basename(filename), imageSize: disc.size,
      partitionOffset: disc.offset, fileCount: files.length,
      totalFileBytes: files.reduce((sum, file) => sum + file.size, 0), files };
    if (command === 'list') console.log(JSON.stringify(manifest, null, 2));
    else {
      fs.mkdirSync(path.dirname(path.resolve(manifestPath)), { recursive: true });
      fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n', { flag: 'wx' });
      console.log(`Extracted ${files.length} files; hashes saved to ${manifestPath}`);
    }
  } finally { disc.close(); }
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  try { main(); } catch (error) { console.error(error.message); process.exitCode = 1; }
}
