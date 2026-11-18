"""Check exact bitmap strikes in the compiled image and native UI samples."""
import hashlib
import json
from PIL import Image, ImageChops
from bitmap_font import SIZES, TOP_PAD, cell, codes
from test_popup import Harness, key, image, R

def main():
    h = Harness()
    glyph_codes = list(codes())
    sampled = 0
    for size in SIZES:
        address = h.s['font' + str(size)]
        stride = (size + 7) // 8 * (size + 8)
        for character in '国确定选择取消苯Wg80%ⅷɑ':
            encoded = character.encode('gbk')
            code = int.from_bytes(encoded, 'big')
            index = glyph_codes.index(code)
            expected, _ = cell(character, size)
            assert h.m.read(address + index * stride, stride) == expected.tobytes(), (character, size)
            sampled += 1
    scaled = 0
    for character in codes().values():
        small, _ = cell(character, 12)
        large, _ = cell(character, 24)
        expected = small.resize((24, 40), Image.Resampling.NEAREST).crop((0, TOP_PAD, 24, 36))
        assert ImageChops.difference(large, expected).getbbox() is None
        scaled += 1
    out = R / 'reports/screenshots'
    h.m.call('h1_draw_main', 0, instruction_limit=30000000)
    image(h.pixels()).save(out / 'mips-bitmap-main.png')
    h.reset()
    h.m.intercept('sango_settings_sdk_write', 2, lambda args: 8)
    h.m.word(h.s['music_volume'], 8)
    h.run('sango_settings_show', [key(1)])
    image(h.frames[0]).save(out / 'mips-bitmap-settings.png')
    h.reset()
    assert h.run('sango_ui_select_number', [key(1)], 1234, 9999) == 65535
    image(h.frames[0]).save(out / 'mips-bitmap-number.png')
    report = {'result': 'passed', 'bda_sha256': hashlib.sha256((R / 'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
              'asset_pack_sha256': hashlib.sha256((R / 'dist/h1/sango.h1r').read_bytes()).hexdigest(),
              'compiled_glyph_samples': sampled, 'exact_integer_scale_checks': scaled,
              'native_sizes': [12, 14, 16], 'title_size': 24, 'fractional_scaling': False,
              'screens': ['main', 'settings-with-percent', 'number-input-1234'],
              'scope': 'Actual compiled MIPS font data and UI; heap/input/settings-write/display modeled. Not physical-device evidence.'}
    (R / 'reports/h1-bitmap-font.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__': main()
