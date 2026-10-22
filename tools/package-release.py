"""Create an allowlisted Windows release ZIP; exclude local game and build data."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def zip_entry(archive, name, data):
    entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    entry.create_system = 0
    entry.external_attr = 32
    entry.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(entry, data)


def check_binary(name, data):
    # Inspect both narrow and wide strings without putting identifiers in output.
    identifiers = {str(ROOT), str(Path.home()), os.environ.get("USERNAME", "")}
    for identifier in identifiers:
        if len(identifier) < 4:
            continue
        for spelling in {identifier, identifier.replace("\\", "/")}:
            if spelling.lower().encode() in data.lower() or spelling.lower().encode("utf-16le") in data.lower():
                raise ValueError(f"Build-machine identifier found in {name}")
    if re.search(rb"(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|sk-proj-[A-Za-z0-9_-]{30,})", data):
        raise ValueError(f"Credential-like string found in {name}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", default="v0.1.0-alpha.9")
    args = parser.parse_args()
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+(?:-[a-z0-9.]+)?", args.version):
        raise ValueError("Invalid release version")
    build = ROOT / "out/build/win-amd64-release"
    release = ROOT / "out/release"
    release.mkdir(parents=True, exist_ok=True)
    sources = release / "ReXGlue-bd833a2-corresponding-source.zip"
    if not sources.is_file():
        raise ValueError("Run bundle-sdk-source.py first")
    files = {name: (build / name).read_bytes() for name in
             ["n3.exe", "n3_import.exe", "rexruntime.dll", "rexgpu-xenos.dll"]}
    # Only the release x64 CRT directory from the official tool download is used.
    crt_packages = sorted((ROOT / ".tools/downloads").glob("Microsoft.VC.*.CRT.Redist.X64.base.vsix"))
    if len(crt_packages) != 1:
        raise ValueError("Expected one Microsoft x64 CRT redistributable archive")
    with zipfile.ZipFile(crt_packages[0]) as crt:
        for name in crt.namelist():
            if "/x64/Microsoft.VC143.CRT/" in name and name.endswith(".dll") and "/debug_nonredist/" not in name:
                files[PurePosixPath(name).name] = crt.read(name)
    for required in ["msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll"]:
        if required not in files:
            raise ValueError("Missing release C/C++ runtime")
    for name, data in files.items():
        check_binary(name, data)
    files["README.txt"] = (ROOT / "docs/RELEASE-README.txt").read_bytes()
    files["THIRD_PARTY.md"] = (ROOT / "THIRD_PARTY.md").read_bytes()
    files["supported-game.json"] = (ROOT / "docs/game-build.json").read_bytes()
    # Preserve all separately supplied upstream notices, including nested libraries.
    with zipfile.ZipFile(sources) as source:
        for name in source.namelist():
            path = PurePosixPath(name)
            if path.name.lower().startswith(("license", "copying", "notice", "copyright", "authors")):
                files["licenses/" + name] = source.read(name)
        # Small vendored libraries keep notices in source headers instead.
        prefixes = ["rexglue-sdk/thirdparty/" + name + "/" for name in
                    ["crypto", "disasm", "disruptorplus", "dxbc", "dxc", "pe", "renderdoc", "stb", "tiny-aes-c"]]
        for name in source.namelist():
            if any(name.startswith(prefix) for prefix in prefixes) and name.endswith((".h", ".hpp", ".c", ".cpp")):
                data = source.read(name)
                if b"license" in data.lower() or b"copyright" in data.lower():
                    files["licenses/embedded-notices/" + name] = data
    files["licenses/Microsoft-runtime.txt"] = b"""Microsoft Visual C++ runtime 14.44, release x64.
Copyright (c) Microsoft Corporation. All rights reserved.
Distributed as application-local runtime dependencies under the applicable
Microsoft Visual Studio license terms and distributable-code list:
https://visualstudio.microsoft.com/license-terms/
https://learn.microsoft.com/visualstudio/releases/2022/redistribution
https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files
No Microsoft development tools or debug runtimes are included.
"""
    hashes = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())}
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    manifest = {"version": args.version, "platform": "Windows x64", "sourceCommit": commit, "files": hashes,
                "sdkSourceArchive": sources.name,
                "sdkSourceSha256": hashlib.sha256(sources.read_bytes()).hexdigest()}
    files["release-manifest.json"] = json.dumps(manifest, indent=2).encode()
    destination = release / f"N3-Recompiled-{args.version}-windows-x64.zip"
    with zipfile.ZipFile(destination, "w", compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            zip_entry(archive, name, data)
    checksums = "".join(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n" for path in [destination, sources])
    (release / "SHA256SUMS.txt").write_text(checksums, encoding="ascii")
    print(f"Packaged {len(files)} allowlisted files: {destination.name} ({destination.stat().st_size} bytes)")
    print("Binary identifier scan passed; no game files, profiles, logs, PDBs, or images admitted")


if __name__ == "__main__":
    main()
