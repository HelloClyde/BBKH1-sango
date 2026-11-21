"""Resample the original color battle frames, using the same masks as the game."""
import argparse
from pathlib import Path
import struct
import zlib

from PIL import Image, ImageDraw

SOURCE_TILE = 16
TILE = 24
TRANSPARENT_ZERO = 0x100
# Mountain interiors on the original 16-pixel grid. The large ridges are drawn across
# their source tile boundaries so the repaired slopes meet at the same pixels.
MOUNTAIN_FACES = (
    ((6,), ((1, 7), (5, 4), (8, 6), (11, 3), (15, 7), (15, 13), (8, 13), (6, 10), (3, 9))),
    ((7,), ((1, 9), (8, 3), (10, 4), (15, 10), (13, 12), (10, 13), (6, 12), (3, 11))),
    ((8,), ((4, 8), (8, 4), (12, 8), (11, 9), (12, 12), (5, 12), (7, 9))),
    ((9,), ((1, 8), (4, 5), (6, 6), (8, 4), (11, 6), (12, 5), (15, 8), (14, 10), (5, 10), (3, 9))),
    ((10, 11, 12, 13), ((0, 5), (5, 1), (10, 4), (13, 4), (16, 0), (18, 1), (23, 6), (31, 10),
                         (29, 12), (23, 13), (29, 17), (29, 21), (25, 22), (31, 31), (24, 31),
                         (19, 31), (15, 30), (9, 30), (6, 27), (2, 27), (3, 24),
                         (0, 22), (0, 20), (4, 19), (9, 13), (5, 11), (0, 13))),
    ((14, 15), ((2, 10), (10, 5), (14, 2), (17, 4), (22, 0), (26, 3), (30, 6),
                (30, 9), (27, 11), (29, 14), (25, 15), (10, 15), (5, 13))),
)


def original_frames(library, color):
    _, size, count, table, payload, lib_size, crc = struct.unpack_from('<8s6I', color)
    if (color[:8], size, table, payload, lib_size, crc) != (
            b'SGCOLOR1', len(color), 32, 32+16*count, len(library), zlib.crc32(library)):
        raise ValueError('The battle LIB and color pack must be a matching pair')
    entries = {source: (offset, w, h, flags) for source, offset, w, h, flags in
               struct.iter_unpack('<IIHHI', color[table:payload])}

    def decode(resource, expected_frames):
        offset, = struct.unpack_from('<I', library, (resource-1)*4)
        width, height, frames, flags = struct.unpack_from('<HHBB', library, offset+12)
        if (width, height, frames) != (SOURCE_TILE, SOURCE_TILE, expected_frames):
            raise ValueError(f'Unexpected battle resource {resource} dimensions')
        mask_bytes = ((width+3)//4)*height
        stride = mask_bytes*(1+(flags&1))
        for frame in range(frames):
            start = offset+18+frame*stride
            pos, w, h, color_flags = entries[start]
            if (w,h)!=(width,height) or (color_flags&1)!=(flags&1):
                raise ValueError(f'Mismatched color frame for battle resource {resource}')
            values = struct.unpack_from(f'<{w*h}H', color, pos)
            pixels = []
            for i, value in enumerate(values):
                if color_flags&TRANSPARENT_ZERO:
                    hidden = value==0
                else:
                    hidden = flags&1 and (library[start+i//4]>>(6-i%4*2))&1
                pixels.append((0,0,0,0) if hidden else
                              ((value>>11)<<3, ((value>>5)&63)<<2, (value&31)<<3, 255))
            yield pixels

    return list(decode(4,46)), list(decode(5,32))


def similar(a,b):
    return a[3]==b[3] and all(abs(a[i]-b[i])<=24 for i in range(3))


def resample(pixels):
    """Refine diagonal edges at 3x, then average 2x2 coverage for the H1 grid."""
    size = SOURCE_TILE*3
    enlarged = Image.new('RGBA',(size,size))

    def at(x,y):
        return pixels[min(max(y,0),SOURCE_TILE-1)*SOURCE_TILE+min(max(x,0),SOURCE_TILE-1)]

    for y in range(SOURCE_TILE):
        for x in range(SOURCE_TILE):
            a,b,c,d,e,f,g,h,i = (at(x+dx,y+dy) for dy in (-1,0,1) for dx in (-1,0,1))
            block = [e]*9
            if not similar(b,h) and not similar(d,f):
                db,bf,dh,hf = similar(d,b),similar(b,f),similar(d,h),similar(h,f)
                block = [d if db else e,
                         b if (db and not similar(e,c)) or (bf and not similar(e,a)) else e,
                         f if bf else e,
                         d if (db and not similar(e,g)) or (dh and not similar(e,a)) else e,
                         e,
                         f if (bf and not similar(e,i)) or (hf and not similar(e,c)) else e,
                         d if dh else e,
                         h if (dh and not similar(e,i)) or (hf and not similar(e,g)) else e,
                         f if hf else e]
            for sub,pixel in enumerate(block):
                enlarged.putpixel((x*3+sub%3,y*3+sub//3),pixel)
    # Pillow averages premultiplied alpha so hidden pixels cannot darken a unit's edge.
    return enlarged.resize((TILE,TILE),Image.Resampling.BOX)


def rgb565(image):
    return [(r>>3)<<11 | (g>>2)<<5 | (b>>3) for r,g,b in image.convert('RGB').get_flattened_data()]


def fill_mountain_faces(tiles):
    """Replace grass embedded inside the original rock faces with neighboring rock colors."""
    for indices, outline in MOUNTAIN_FACES:
        columns = min(len(indices), 2)
        size = (columns * SOURCE_TILE, len(indices) // columns * SOURCE_TILE)
        source = Image.new('RGBA', size)
        positions = []
        for slot, index in enumerate(indices):
            row, col = divmod(slot, columns)
            position = (col * SOURCE_TILE, row * SOURCE_TILE)
            tile = Image.new('RGBA', (SOURCE_TILE, SOURCE_TILE))
            tile.putdata(tiles[index])
            source.paste(tile, position)
            positions.append(position)

        face = Image.new('1', size)
        ImageDraw.Draw(face).polygon(outline, fill=1)
        grass = Image.new('1', size)
        grass.putdata([g > r and g - b >= 24 for r, g, b, _ in source.get_flattened_data()])
        repaired = source.copy()
        for y in range(size[1]):
            for x in range(size[0]):
                if not (face.getpixel((x, y)) and grass.getpixel((x, y))):
                    continue
                weighted = [0.0, 0.0, 0.0]
                weight_sum = 0.0
                for sy in range(max(0, y - 5), min(y + 6, size[1])):
                    for sx in range(max(0, x - 5), min(x + 6, size[0])):
                        if grass.getpixel((sx, sy)):
                            continue
                        weight = 1 / ((sx - x) ** 2 + (sy - y) ** 2)
                        for channel, color in enumerate(source.getpixel((sx, sy))[:3]):
                            weighted[channel] += weight * color
                        weight_sum += weight
                if not weight_sum:
                    raise ValueError(f'Missing rock sample for mountain tiles {indices} at {(x, y)}')
                repaired.putpixel((x, y), (*[round(value / weight_sum) for value in weighted], 255))

        for index, (x, y) in zip(indices, positions):
            tiles[index] = repaired.crop((x, y, x + SOURCE_TILE, y + SOURCE_TILE)).get_flattened_data()


def unit_bounds(frame):
    return frame.getchannel('A').point(lambda value: 255 if value >= 128 else 0).getbbox()


def write_art(destination, library, color, preview=None, unit_strip=None):
    source_tiles, source_units = original_frames(library,color)
    fill_mountain_faces(source_tiles)
    terrain = [resample(frame) for frame in source_tiles]
    units = [resample(frame) for frame in source_units]
    if unit_strip is not None:
        with Image.open(unit_strip) as strip:
            if strip.mode!='RGBA' or strip.size!=(TILE,TILE*len(units)):
                raise ValueError('Invalid restored battle unit strip dimensions or mode')
            for index, baseline in enumerate(units):
                frame = strip.crop((0,index*TILE,TILE,(index+1)*TILE))
                # Keep the original placement and full footprint, while allowing a redrawn silhouette.
                if index % 16 < 14:
                    if frame.getchannel('A').getbbox() != unit_bounds(baseline):
                        raise ValueError(f'Restoration changed battle unit frame {index} bounds')
                elif frame.tobytes() != baseline.tobytes():
                    raise ValueError(f'Restoration changed battle state frame {index}')
                units[index] = frame
    with destination.open('w',encoding='ascii',newline='\n') as out:
        out.write('/* Generated by h1/prepare_battle_art.py. */\n')
        for kind,name,frames in (('uint16_t','battle_terrain_tiles',map(rgb565,terrain)),
                                  ('uint16_t','battle_unit_colors',map(rgb565,units)),
                                  ('uint8_t','battle_unit_alpha',(im.getchannel('A').tobytes() for im in units))):
            out.write(f'static const {kind} {name}[][{TILE*TILE}] = {{\n')
            for frame in frames:
                out.write('    {\n')
                for start in range(0,len(frame),12):
                    out.write('        '+', '.join(hex(v) for v in frame[start:start+12])+',\n')
                out.write('    },\n')
            out.write('};\n')
    if preview:
        image=Image.new('RGB',(12*TILE,7*TILE),(188,204,122))
        for index,frame in enumerate(terrain+units):
            image.paste(frame,(index%12*TILE,index//12*TILE),frame)
        image.save(preview)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--library',required=True,type=Path)
    parser.add_argument('--colors',required=True,type=Path)
    parser.add_argument('--preview',type=Path)
    parser.add_argument('--unit-strip',type=Path)
    args=parser.parse_args()
    write_art(args.output,args.library.read_bytes(),args.colors.read_bytes(),args.preview,args.unit_strip)
