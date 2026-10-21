"""Run the pinned downloader with cleanup confined to project-local tool folders."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import sys

tools = Path(__file__).resolve().parent
local = tools.parent / '.tools'
lock = json.loads((tools / 'toolchain.lock.json').read_text())['msvc_downloader']
downloader = Path(sys.argv[1]).resolve()
if hashlib.sha256(downloader.read_bytes()).hexdigest() != lock['sha256']:
    raise RuntimeError('Downloader checksum mismatch')
os.chdir(local)
allowed = [(local / name).resolve() for name in ('msvc', 'downloads')]
original_rmtree = shutil.rmtree


def guarded_rmtree(target, *args, **kwargs):
    resolved = Path(target).resolve()
    if not any(resolved == root or root in resolved.parents for root in allowed):
        raise RuntimeError(f'Refusing cleanup outside tool folders: {resolved}')
    return original_rmtree(target, *args, **kwargs)


shutil.rmtree = guarded_rmtree
sys.argv = [str(downloader), '--vs', lock['vs'], '--msvc-version', lock['msvc_version'],
            '--sdk-version', lock['sdk_version'], '--accept-license']
runpy.run_path(str(downloader), run_name='__main__')
