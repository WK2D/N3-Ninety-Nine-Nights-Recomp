"""Publish a verified release using the existing Git credential helper session."""
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
REPO = "WK2D/N3-Ninety-Nine-Nights-Recomp"
TAG = "v0.1.0-alpha.1"
API = "https://api.github.com/repos/" + REPO


def main():
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT).strip():
        raise RuntimeError("Commit source changes before publishing")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    remote = subprocess.check_output(["git", "ls-remote", "origin", "refs/heads/main"], cwd=ROOT, text=True).split()[0]
    if remote != commit:
        raise RuntimeError("Push the exact source commit before publishing")
    folder = ROOT / "out/release"
    sums = (folder / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines()
    assets = []
    for line in sums:
        digest, name = line.split("  ", 1)
        if Path(name).name != name:
            raise RuntimeError("Invalid asset filename")
        path = folder / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError("Release asset checksum changed")
        assets.append(path)
    assets.append(folder / "SHA256SUMS.txt")
    # Keep credentials in memory and send them only to GitHub's API/upload hosts.
    result = subprocess.run(["git", "credential", "fill"], input="protocol=https\nhost=github.com\n\n",
                            cwd=ROOT, capture_output=True, text=True, check=True)
    credentials = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    token = credentials.get("password")
    if not token:
        raise RuntimeError("GitHub credential helper returned no credential")

    def request(method, url, data=None, content_type="application/json"):
        if urllib.parse.urlparse(url).hostname not in {"api.github.com", "uploads.github.com"}:
            raise RuntimeError("Unexpected GitHub API host")
        headers = {"Authorization": "Bearer " + token, "User-Agent": "N3-release-tools",
                   "Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"}
        if data is not None:
            headers["Content-Type"] = content_type
            if content_type == "application/json":
                data = json.dumps(data).encode()
        req = urllib.request.Request(url, data=data, headers=headers, method=method)
        with urllib.request.urlopen(req, timeout=180) as response:
            return json.load(response)

    try:
        release = request("GET", API + "/releases/tags/" + TAG)
        if not release["draft"]:
            raise RuntimeError("This release is already public; no assets were changed")
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise RuntimeError(f"GitHub release lookup failed (HTTP {error.code})") from None
        release = request("POST", API + "/releases", {
            "tag_name": TAG, "target_commitish": commit, "name": "N3 Recompiled " + TAG,
            "body": (ROOT / f"docs/RELEASE-{TAG}.md").read_text(encoding="utf-8"),
            "draft": True, "prerelease": True})
    uploaded = {asset["name"]: asset for asset in release["assets"]}
    for path in assets:
        expected = "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
        if path.name in uploaded:
            if uploaded[path.name].get("digest") != expected:
                raise RuntimeError("A draft asset differs; inspect it before resuming")
            continue
        url = release["upload_url"].split("{")[0] + "?name=" + urllib.parse.quote(path.name)
        asset = request("POST", url, path.read_bytes(), "application/octet-stream")
        if asset.get("digest") != expected:
            raise RuntimeError("GitHub asset digest mismatch; draft was not published")
        print(f"Uploaded and verified {path.name}")
    release = request("GET", API + "/releases/" + str(release["id"]))
    if {asset["name"] for asset in release["assets"]} != {path.name for path in assets}:
        raise RuntimeError("Draft has unexpected assets; inspect it before publishing")
    public = request("PATCH", API + "/releases/" + str(release["id"]), {"draft": False, "prerelease": True})
    print("Published " + public["html_url"])


if __name__ == "__main__":
    main()
