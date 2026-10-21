"""Bundle the pinned SDK and its non-Vulkan dependency sources for binary releases."""
import concurrent.futures
import configparser
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent
COMMIT = "bd833a2ab69d305f99ddf57418fc38ffec8af93d"
OUT = ROOT / "out" / "release"
CACHE = ROOT / ".tools" / "source-archives"
REQUIRED = {"cli11", "libmspack", "FFmpeg", "tomlplusplus", "simde", "xxHash",
            "spdlog", "fmt", "utfcpp", "imgui", "sdl3", "inja", "o1heap", "tracy", "catch2"}


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "N3-release-tools"})
    with urllib.request.urlopen(req, timeout=120) as response:
        return response.read()


def snapshot(repo, commit):
    CACHE.mkdir(parents=True, exist_ok=True)
    cache = CACHE / (repo.replace("/", "-") + "-" + commit + ".tar.gz")
    url = f"https://codeload.github.com/{repo}/tar.gz/{commit}"
    if not cache.exists():
        data = fetch(url)
        temp = cache.with_suffix(".tmp")
        temp.write_bytes(data)
        temp.replace(cache)
    data = cache.read_bytes()
    return data, {"repository": f"https://github.com/{repo}", "commit": commit,
                  "archive": url, "sha256": hashlib.sha256(data).hexdigest()}


def add_snapshot(archive, data, prefix):
    # Never extract archive paths to disk. Only regular files inside one root
    # are admitted to the new source zip; symlinks and archive metadata are omitted.
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as source:
        for member in source:
            path = PurePosixPath(member.name)
            if not member.isfile() or len(path.parts) < 2:
                continue
            relative = PurePosixPath(*path.parts[1:])
            if relative.is_absolute() or ".." in relative.parts or ".git" in relative.parts:
                raise ValueError("Unsafe source archive path")
            contents = source.extractfile(member).read()
            add_file(archive, prefix + "/" + relative.as_posix(), contents)


def add_file(archive, name, data):
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sdk, sdk_manifest = snapshot("rexglue/rexglue-sdk", COMMIT)
    tree = json.loads(fetch(f"https://api.github.com/repos/rexglue/rexglue-sdk/git/trees/{COMMIT}?recursive=1"))
    modules = {item["path"]: item["sha"] for item in tree["tree"] if item["type"] == "commit"}
    with tarfile.open(fileobj=io.BytesIO(sdk), mode="r:gz") as archive:
        member = next(m for m in archive if m.name.endswith("/.gitmodules"))
        config = configparser.ConfigParser()
        config.read_string(archive.extractfile(member).read().decode())
    jobs = []
    for section in config.sections():
        path = config[section]["path"]
        if path.rsplit("/", 1)[-1] in REQUIRED:
            repo = config[section]["url"].removesuffix(".git").removeprefix("https://github.com/")
            jobs.append((path, repo, modules[path]))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda job: (job[0], *snapshot(job[1], job[2])), jobs))
    destination = OUT / "ReXGlue-bd833a2-corresponding-source.zip"
    with zipfile.ZipFile(destination, "w", compresslevel=6) as archive:
        add_snapshot(archive, sdk, "rexglue-sdk")
        for path, data, manifest in results:
            manifest["path"] = path
            add_snapshot(archive, data, "rexglue-sdk/" + path)
        add_file(archive, "SOURCE-MANIFEST.json", json.dumps([sdk_manifest] + [r[2] for r in results], indent=2).encode())
        add_file(archive, "README.txt", b"""Corresponding source for the ReXGlue runtime and GPU DLLs.
SDK commit bd833a2ab69d305f99ddf57418fc38ffec8af93d; upstream sources unmodified.
Non-Vulkan dependencies are included at the SDK's pinned commits. License texts
and build scripts are preserved in each source directory. No game material is
included. FFmpeg and libmspack are LGPL components; see their COPYING files.

Rebuilding: consult rexglue-sdk/README.md and CMakePresets.json. On Windows use
Clang 20+ and VS 2022 x64 tools. Initialize a local Git repository in each of
the included thirdparty submodule directories (git init) to satisfy the SDK's
source checkout presence checks. Configure with REXGLUE_USE_VULKAN=OFF and build
the Release configuration. FFmpeg is built by the SDK's supplied CMake scripts.
Replace the released rexruntime.dll and rexgpu-xenos.dll with compatible rebuilt
DLLs to use modified SDK/library code. This application does not restrict their
replacement. Reverse engineering for debugging modifications to LGPL components
is permitted. The project's own source is available at the matching GitHub tag;
recompiling the game translation requires a matching executable from your disc.

Build recipe used by the prebuilt SDK is preserved in
rexglue-sdk/.github/workflows/build-win-amd64.yaml,
rexglue-sdk/.github/workflows/_build-platform.yaml and their referenced scripts.
SOURCE-MANIFEST.json records source URLs, commit IDs, and archive SHA-256 values.
""")
    print(f"Bundled SDK and {len(results)} dependencies: {destination.name} ({destination.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
