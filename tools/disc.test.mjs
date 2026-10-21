import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import { Disc } from './disc.mjs';

function fixture(t, change = () => {}) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'n3-disc-test-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const image = Buffer.alloc(40 * 2048);
  image.write('MICROSOFT*XBOX*MEDIA', 32 * 2048, 'ascii');
  image.write('MICROSOFT*XBOX*MEDIA', 33 * 2048 - 20, 'ascii');
  image.writeUInt32LE(33, 32 * 2048 + 20);
  image.writeUInt32LE(2048, 32 * 2048 + 24);
  function entry(sector, name, dataSector, size, attributes = 0) {
    const start = sector * 2048;
    image.writeUInt32LE(dataSector, start + 4);
    image.writeUInt32LE(size, start + 8);
    image[start + 12] = attributes;
    image[start + 13] = name.length;
    image.write(name, start + 14, 'latin1');
  }
  entry(33, 'data', 34, 2048, 0x10);
  entry(34, 'hello.txt', 35, 5);
  image.write('hello', 35 * 2048);
  change(image);
  const filename = path.join(root, 'test.iso');
  fs.writeFileSync(filename, image);
  return { filename, root };
}

test('walk nested XDVDFS entries and extract exact bytes with a hash', t => {
  const { filename, root } = fixture(t);
  const disc = new Disc(filename);
  try {
    const entries = disc.entries();
    assert.deepEqual(entries, [{ path: 'data/hello.txt', offset: 35 * 2048, size: 5 }]);
    const hash = disc.extract(entries[0], path.join(root, 'output'));
    assert.equal(fs.readFileSync(path.join(root, 'output/data/hello.txt'), 'utf8'), 'hello');
    assert.equal(hash, crypto.createHash('sha256').update('hello').digest('hex'));
    assert.throws(() => disc.extract(entries[0], path.join(root, 'output')), /EEXIST/);
  } finally { disc.close(); }
});

test('reject path traversal names', t => {
  const { filename } = fixture(t, data => { data[34 * 2048 + 13] = 2; data.write('..', 34 * 2048 + 14); });
  const disc = new Disc(filename);
  try { assert.throws(() => disc.entries(), /Unsafe entry name/); } finally { disc.close(); }
});

test('reject cyclic directory links', t => {
  const { filename } = fixture(t, data => {
    const start = 34 * 2048;
    data.writeUInt16LE(8, start);
    data.writeUInt16LE(8, start + 32);
    data.writeUInt32LE(35, start + 36);
    data[start + 45] = 1;
    data.write('x', start + 46);
  });
  const disc = new Disc(filename);
  try { assert.throws(() => disc.entries(), /Directory node cycle/); } finally { disc.close(); }
});

test('reject extents beyond the image', t => {
  const { filename } = fixture(t, data => data.writeUInt32LE(100, 34 * 2048 + 4));
  const disc = new Disc(filename);
  try { assert.throws(() => disc.entries(), /Extent outside disc/); } finally { disc.close(); }
});

test('reject a damaged volume descriptor', t => {
  const { filename } = fixture(t, data => { data[33 * 2048 - 1] = 0; });
  assert.throws(() => new Disc(filename), /Invalid volume footer/);
});
