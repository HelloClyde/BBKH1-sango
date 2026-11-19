"""Build H1 bitmap cells from native BDF strikes and 24px brush titles."""
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / 'assets/fonts/bitmap'
TITLE_FONT = FONT_DIR.parent / 'MaShanZheng-Regular.ttf'
TOP_PAD, EXTRA_ROWS = 4, 8
SIZES = (12, 14, 16, 24)
WQY = {12: 'wenquanyi_9pt.bdf', 14: 'wenquanyi_13px.bdf', 16: 'wenquanyi_12pt.bdf'}

@dataclass(frozen=True)
class Glyph:
    width: int
    height: int
    x: int
    y: int
    advance: int
    rows: tuple

    def points(self):
        stride = (self.width + 7) // 8 * 8
        return [(x + self.x, self.y + self.height - 1 - y)
                for y, row in enumerate(self.rows) for x in range(self.width)
                if row & (1 << (stride - 1 - x))]

@lru_cache(maxsize=None)
def read_bdf(name):
    glyphs = {}
    with (FONT_DIR / name).open(encoding='latin1') as source:
        code = -1
        rows = None
        for raw in source:
            line = raw.strip()
            if line.startswith('STARTCHAR '): code, rows = -1, None
            elif line.startswith('ENCODING '): code = int(line.split()[1])
            elif line.startswith('DWIDTH '): advance = int(line.split()[1])
            elif line.startswith('BBX '): width, height, x, y = map(int, line.split()[1:])
            elif line == 'BITMAP': rows = []
            elif line == 'ENDCHAR':
                if code >= 0:
                    assert len(rows) == height, (name, code)
                    glyphs[code] = Glyph(width, height, x, y, advance, tuple(rows))
                rows = None
            elif rows is not None: rows.append(int(line, 16))
    return glyphs

def codes():
    result = {i: chr(i) for i in range(32, 127)}
    for a in range(0xa1, 0xf8):
        for b in range(0xa1, 0xff):
            try: result[a << 8 | b] = bytes([a, b]).decode('gbk')
            except UnicodeDecodeError: pass
    return result

def glyph_points(character, size):
    cp = ord(character)
    if cp < 128:
        return read_bdf(f'ter-u{size}n.bdf')[cp].points(), 'native-ascii'
    font = read_bdf(WQY[size])
    if cp in font: return font[cp].points(), 'native-cjk'
    if 0x2170 <= cp <= 0x2179:
        value = ('i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix', 'x')[cp - 0x2170]
        points, cursor = [], 0
        for letter in value:
            ink = font[ord(letter)].points()
            left = min(x for x, y in ink); right = max(x for x, y in ink)
            points.extend((x - left + cursor, y) for x, y in ink)
            cursor += right - left + 2
        assert cursor - 1 <= size, (character, size, cursor)
        return [(x + (size - cursor + 1) // 2, y) for x, y in points], 'composed-roman'
    for other in (12, 16):
        source = read_bdf(WQY[other])
        if cp in source:
            ink = source[cp].points()
            left = min(x for x, y in ink); right = max(x for x, y in ink)
            if right - left + 1 <= size:
                return [(x - left + (size - right + left - 1) // 2, y) for x, y in ink], f'native-{other}px-fallback'
    raise ValueError(('Bitmap glyph unavailable', character, size))

@lru_cache(maxsize=1)
def title_font():
    font = ImageFont.truetype(str(TITLE_FONT), 24)
    missing = '\U0010FFFF'
    return font, font.getbbox(missing, anchor='ls'), bytes(font.getmask(missing))


def brush_title(character):
    font, missing_bounds, missing_mask = title_font()
    size = 24
    height = size + EXTRA_ROWS
    bounds = font.getbbox(character, anchor='ls')
    if bounds == missing_bounds and bytes(font.getmask(character)) == missing_mask:
        return None
    left, top, right, bottom = bounds
    if right - left > size or bottom - top > height:
        return None
    x = min(max(round((size - font.getlength(character)) / 2), -left), size - right)
    y = min(max(TOP_PAD + size - 2, -top), height - bottom)
    mask = Image.new('L', (size, height))
    ImageDraw.Draw(mask).text((x, y), character, 255, font=font, anchor='ls')
    # Threshold without dithering to match the 1-bit pixels drawn by H1.
    bitmap = mask.point(lambda value: 255 if value >= 100 else 0, '1')
    return bitmap if bitmap.getbbox() else None


def cell(character, size):
    assert size in SIZES
    if size == 24 and '\u4e00' <= character <= '\u9fff':
        title = brush_title(character)
        if title is not None:
            return title, 'brush-title'
    scale = 2 if size == 24 else 1
    native = size // scale
    points, source = glyph_points(character, native)
    width = size // 2 if ord(character) < 128 else size
    if points:
        left = min(x for x, y in points); right = max(x for x, y in points)
        assert right - left + 1 <= width // scale
        shift = -left if left < 0 else width // scale - 1 - right if right >= width // scale else 0
        if shift:
            points = [(x + shift, y) for x, y in points]
            source += '-bearing-shift'
    im = Image.new('1', (size, size + EXTRA_ROWS))
    baseline = (native - 2) * scale
    for x, y in points:
        for dy in range(scale):
            for dx in range(scale):
                xx, yy = x * scale + dx, TOP_PAD + baseline - y * scale + dy
                assert 0 <= xx < width and 0 <= yy < im.height, (character, size, xx, yy, source)
                im.putpixel((xx, yy), 1)
    assert sum(bool(p) for p in im.getdata()) == len(points) * scale * scale
    return im, source

def text_mask(text, size):
    width = sum(size // 2 if ord(c) < 128 else size for c in text)
    out = Image.new('1', (width, size + EXTRA_ROWS))
    x = 0
    for character in text:
        im, _ = cell(character, size)
        out.paste(im, (x, 0))
        x += size // 2 if ord(character) < 128 else size
    return out
