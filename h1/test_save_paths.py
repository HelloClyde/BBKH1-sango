"""Check the compiled filesystem boundary for all three paired save slots."""
import hashlib
import json
import struct
import test_mips as base

def main():
    m = base.H1Machine(base.ELF)
    buf = base.module.BUFFER
    directory = 'A:\\应用\\数据\\游戏'.encode('gbk')
    calls = []
    def string(address):
        return m.read(address, 256).split(b'\0')[0]
    def service(offset, count, function):
        address = 0x80001000 + offset * 4
        name = 'filesystem_' + str(offset)
        m.word(0x80800000 + offset, address)
        m.write(address, bytes.fromhex('0800e00300000000'))
        m.symbols[name] = address
        m.intercept(name, count, function)
    m.word(0x83c00008, 0x80800000)
    def opened(args):
        calls.append(('open', string(args[0]), string(args[1])))
        return 0x816f0000
    def mkdir(args):
        calls.append(('mkdir', string(args[0])))
        return 0
    service(0, 2, opened)
    service(0x30, 1, mkdir)
    checks = 0
    for slot in range(6):
        filename = f'sango{slot}.sav'.encode()
        expected = directory + b'\\' + filename
        for prefix in (b'', b'A:\\H1SANGO\\', '系统\\数据\\游戏\\'.encode('gbk')):
            m.write(buf, prefix + filename + b'\0')
            for original_mode, native_mode in ((b'rbf', b'rb'), (b'wbf', b'wb')):
                m.write(buf + 256, original_mode + b'\0')
                assert m.call('bda_sango_fs_fopen', buf, buf + 256) == 0x816f0000
                assert calls[-1] == ('open', expected, native_mode), calls[-1]
                checks += 1
    for name, expected in [(b'sango.cfg', directory + b'\\sango.cfg'),
                           (b'sango.lib', directory + b'\\sango.lib'),
                           (b'sango-music\\menu.wav', directory + b'\\SGMUSIC\\menu.wav')]:
        m.write(buf, name + b'\0'); m.write(buf + 256, b'rb\0')
        m.call('bda_sango_fs_fopen', buf, buf + 256)
        assert calls[-1] == ('open', expected, b'rb'), calls[-1]
        checks += 1
    m.write(buf, b'A:\\H1SANGO\0')
    m.call('bda_sango_fs_mkdir', buf)
    assert calls[-1] == ('mkdir', directory)
    checks += 1
    report = {'result': 'passed', 'checks': checks, 'bda_sha256': hashlib.sha256((base.ROOT / 'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
              'directory': directory.decode('gbk'), 'scope': 'Compiled MIPS open/read-mode/write-mode/mkdir service calls; firmware filesystem modeled. Six save files share game-data with resources and settings.'}
    (base.ROOT / 'reports/h1-save-paths.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
