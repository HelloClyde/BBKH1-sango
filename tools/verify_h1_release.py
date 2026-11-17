"""Validate release files against exact physical-H1 evidence; never run firmware here."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / 'dist/h1'
sys.path.insert(0, str(ROOT / 'sdk'))
from h1_bda.validate import validate_bda

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    evidence = ROOT / 'reports/hardware-savepath-20261117'
    checks = json.loads((evidence / 'physical-checks.json').read_text(encoding='utf8'))
    transfers = json.loads((evidence / 'deployment-summary.json').read_text(encoding='utf8'))
    assert checks['result'] == 'passed' and checks['physical_device_used'] is True
    expected = {'H1Sango.bda': checks['bda_sha256'], 'sango.h1r': checks['asset_pack_sha256']}
    for name in ('sango.lib', 'sango.clr', 'menu.wav', 'strategy.wav', 'battle.wav'):
        assert transfers[name]['completed'] and transfers[name]['device_crc_verified']
        expected[name] = transfers[name]['sha256']
    assert validate_bda(DIST / 'H1Sango.bda')['ok']
    meta = json.loads((DIST / 'BUILD_INFO.json').read_text(encoding='utf8'))
    assert meta['sdk_commit'] == '067fe072477861dfc8949d7b1a55279fb92d2548'
    files = {'应用/程序/H1Sango.bda': DIST / 'H1Sango.bda'}
    files.update({'应用/数据/游戏/' + n: DIST / n for n in ('sango.lib', 'sango.clr', 'sango.h1r')})
    files.update({'应用/数据/游戏/SGMUSIC/' + n: ROOT / 'assets/music/runtime' / n
                  for n in ('menu.wav', 'strategy.wav', 'battle.wav')})
    with zipfile.ZipFile(DIST / 'H1Sango-install.zip') as archive:
        assert archive.testzip() is None
        assert len(archive.namelist()) == len(set(archive.namelist()))
        assert not any(n.lower().endswith(('.sav', '.nand', '.img')) for n in archive.namelist())
        for member, source in files.items():
            name = source.name
            assert digest(source) == expected[name], ('physical evidence mismatch', name)
            assert hashlib.sha256(archive.read(member)).hexdigest() == expected[name], member
    for line in (DIST / 'SHA256SUMS.txt').read_text(encoding='ascii').splitlines():
        value, name = line.split('  ', 1)
        assert digest(DIST / name) == value, name
    print('BDA, SDK pin, all seven installed payloads, ZIP and SHA256SUMS match physical-H1 evidence.')

if __name__ == '__main__':
    main()
