"""Install the Windows GNU MIPS 15.2.0 toolchain with a pinned archive digest."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://static.grumpycoder.net/pixel/mips/g++-mipsel-none-elf-15.2.0.zip'
DIGEST = '8ba866e25c9826ee04ab4310365d264e3e73769e3738bb58ae38fd6740b7ee8d'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, help='Use a cached archive instead of downloading')
    parser.add_argument('--dest', type=Path, default=ROOT / '.toolchain')
    args = parser.parse_args()
    dest = args.dest.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    archive = args.archive or dest / 'g++-mipsel-none-elf-15.2.0.zip'
    if args.archive and not archive.is_file():
        raise FileNotFoundError('--archive must name an existing cached ZIP')
    if not archive.exists():
        partial = archive.with_suffix('.part')
        # The distributor rejects urllib's generic Python user agent.
        request = urllib.request.Request(URL, headers={
            'User-Agent': 'BBKH1-sango/0.2.1 (+https://github.com/HelloClyde/BBKH1-sango)',
            'Accept': 'application/octet-stream',
        })
        with urllib.request.urlopen(request, timeout=60) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        partial.replace(archive)
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    if digest != DIGEST:
        raise SystemExit('Toolchain SHA256 mismatch; nothing extracted')
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.infolist():
            if not (dest / member.filename).resolve().is_relative_to(dest):
                raise SystemExit('Unsafe archive path')
        bundle.extractall(dest)
    for name in ('gcc', 'ld', 'nm', 'objcopy'):
        if not (dest / f'bin/mipsel-none-elf-{name}.exe').is_file():
            raise SystemExit('Incomplete toolchain: ' + name)
    print(subprocess.check_output([str(dest / 'bin/mipsel-none-elf-gcc.exe'), '--version'], text=True).splitlines()[0])

if __name__ == '__main__':
    main()
