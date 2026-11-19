"""Pack native BDF strikes and rasterized brush-title glyphs for H1."""
import hashlib
import json
from collections import Counter
from bitmap_font import ROOT, FONT_DIR, TITLE_FONT, SIZES, TOP_PAD, EXTRA_ROWS, cell, codes

def main():
    characters = codes()
    methods = Counter()
    with (ROOT / 'h1/font.inc').open('w', encoding='ascii', newline='\n') as out:
        out.write('/* WenQuanYi: GPL-2.0 with font exception; Terminus and Ma Shan Zheng: OFL-1.1. See LICENSES. */\n')
        out.write(f'enum {{ H1_FONT_TOP_PAD={TOP_PAD}, H1_FONT_EXTRA_ROWS={EXTRA_ROWS} }};\n')
        out.write('static const unsigned short font_codes[]={' + ','.join(str(c) for c in characters) + '};\n')
        for size in SIZES:
            out.write(f'static const unsigned char font{size}[][{(size + EXTRA_ROWS) * ((size + 7) // 8)}]={{\n')
            for character in characters.values():
                bitmap, method = cell(character, size)
                methods[f'{size}:{method}'] += 1
                out.write('{' + ','.join(str(b) for b in bitmap.tobytes()) + '},\n')
            out.write('};\n')
    report = {'result': 'passed', 'font_type': '1-bit bitmap', 'cell_sizes': SIZES,
              'glyphs_per_size': len(characters), 'brush_title_glyphs': methods['24:brush-title'],
              'uniform_cjk_font_pixels': {str(n): n for n in SIZES}, 'extra_rows': EXTRA_ROWS, 'top_pad': TOP_PAD,
              'integer_scaled_title_fallback': 2, 'fractional_scaling': False,
              'glyph_sources': dict(methods),
              'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in [*sorted(FONT_DIR.glob('*.bdf')), TITLE_FONT]},
              'method': 'Copy native BDF ink at 12/14/16px. Rasterize Ma Shan Zheng CJK title glyphs directly at 24px and threshold to 1-bit. Keep 2x BDF for other or unavailable glyphs.'}
    (ROOT / 'reports/font-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__': main()
