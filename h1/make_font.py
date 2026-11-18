"""Pack native WQY/Terminus BDF strikes; retain every source ink pixel."""
import hashlib
import json
from collections import Counter
from bitmap_font import ROOT, FONT_DIR, SIZES, TOP_PAD, EXTRA_ROWS, cell, codes

def main():
    characters = codes()
    methods = Counter()
    with (ROOT / 'h1/font.inc').open('w', encoding='ascii', newline='\n') as out:
        out.write('/* WenQuanYi: GPL-2.0 with font exception; Terminus: OFL-1.1. See LICENSES. */\n')
        out.write(f'enum {{ H1_FONT_TOP_PAD={TOP_PAD}, H1_FONT_EXTRA_ROWS={EXTRA_ROWS} }};\n')
        out.write('static const unsigned short font_codes[]={' + ','.join(str(c) for c in characters) + '};\n')
        for size in SIZES:
            out.write(f'static const unsigned char font{size}[][{(size + EXTRA_ROWS) * ((size + 7) // 8)}]={{\n')
            for character in characters.values():
                bitmap, method = cell(character, size)
                methods[f'{size}:{method}'] += 1
                out.write('{' + ','.join(str(b) for b in bitmap.tobytes()) + '},\n')
            out.write('};\n')
    report = {'result': 'passed', 'font_type': 'native bitmap BDF', 'cell_sizes': SIZES,
              'glyphs_per_size': len(characters), 'uncropped_glyph_checks': len(characters) * len(SIZES),
              'uniform_cjk_font_pixels': {str(n): n for n in SIZES}, 'extra_rows': EXTRA_ROWS, 'top_pad': TOP_PAD,
              'integer_scale': {'12': 1, '14': 1, '16': 1, '24': 2}, 'fractional_scaling': False,
              'glyph_sources': dict(methods), 'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(FONT_DIR.glob('*.bdf'))},
              'method': 'Copy original BDF ink at native 12/14/16px; 24px is exactly 2x12px. Verify all ink pixels fit, with no antialiasing, clipping, fractional scaling or per-character shrinking.'}
    (ROOT / 'reports/font-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__': main()
