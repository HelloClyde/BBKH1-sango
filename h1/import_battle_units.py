"""Import the approved GPT Image battle sheets into the H1 sprite grid."""

import argparse
from pathlib import Path

from PIL import Image, ImageChops

from prepare_battle_art import TILE, original_frames, resample, unit_bounds


SHEETS = (
    (0, 1, 2, 3, 16, 17, 18, 19),
    (4, 5, 6, 7, 20, 21, 22, 23),
    (8, 9, 10, 11, 24, 25, 26, 27),
    (12, 13, 28, 29),
)


def import_units(sheets, library, colors, output):
    _, pixels = original_frames(library.read_bytes(), colors.read_bytes())
    frames = [resample(frame) for frame in pixels]
    for number, indices in enumerate(SHEETS, 1):
        with Image.open(sheets / f'smooth-{number}.png') as source:
            sheet = source.convert('RGB')
        columns = len(indices) // 2
        for slot, index in enumerate(indices):
            row, col = divmod(slot, columns)
            cell = sheet.crop((col * sheet.width // columns, row * sheet.height // 2,
                               (col + 1) * sheet.width // columns, (row + 1) * sheet.height // 2))
            # Key out the nearly uniform slate, including the spaces inside bows and between legs.
            distance = ImageChops.difference(cell, Image.new('RGB', cell.size, (39, 45, 53)))
            red, green, blue = distance.split()
            maximum = ImageChops.lighter(ImageChops.lighter(red, green), blue)
            alpha = maximum.point(lambda value: 255 if value >= 15 else 0)
            bounds = alpha.getbbox()
            if (bounds is None or bounds[0] == 0 or bounds[1] == 0
                    or bounds[2] == cell.width or bounds[3] == cell.height):
                raise ValueError(f'Unexpected foreground bounds for battle frame {index}: {bounds}')
            cell.putalpha(alpha)

            left, top, right, bottom = unit_bounds(frames[index])
            # Average colors with alpha in one pass at the LCD size, without sampling slate pixels.
            sprite = cell.crop(bounds).resize((right - left, bottom - top), Image.Resampling.BOX)
            frame = Image.new('RGBA', (TILE, TILE))
            frame.paste(sprite, (left, top))
            frames[index] = frame

    strip = Image.new('RGBA', (TILE, TILE * len(frames)))
    for index, frame in enumerate(frames):
        strip.paste(frame, (0, index * TILE))
    strip.save(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sheets', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--colors', required=True, type=Path)
    args = parser.parse_args()
    import_units(args.sheets, args.library, args.colors, args.output)
