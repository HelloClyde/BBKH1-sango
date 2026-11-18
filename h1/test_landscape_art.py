"""Exercise the new H1R deck through the real compiled menu and ruler pages."""
import hashlib
import json
import re
import struct
import zlib

from test_popup import B, Harness, R, image, key
from test_mips import module


def pixels565(picture):
    return b''.join(struct.pack('<H', ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
                    for r, g, b in picture.convert('RGB').getdata())


def draw_tests():
    h = Harness()
    m = h.m
    out = R / 'reports/screenshots'
    root = R / 'assets/h1-landscape/native'
    from PIL import Image

    title = None
    for selected in range(4):
        m.call('h1_draw_main', selected)
        screen = image(h.pixels())
        with Image.open(root / f'main-{selected}.png') as preview:
            assert h.pixels() == pixels565(preview)
        current = screen.crop((0, 0, 480, 48)).tobytes()
        if title is None:
            title = current
            screen.save(out / 'mips-art-main.png')
        else:
            assert title == current

    h.pending.append(key(0x1c))
    assert m.call('sango_menu_select_animated',45,0)==0
    assert not h.pending
    screen=image(h.pixels())
    with Image.open(root/'scenarios.png') as expected:
        for x in (16,132,248,364):
            assert pixels565(screen.crop((x,56,x+100,143))) == pixels565(expected.crop((x,56,x+100,143)))
    screen.save(out/'mips-art-periods.png')

    mapping=(R/'include/sango_ruler_portraits.inc').read_text(encoding='utf8')
    rows=[list(map(int,re.findall(r'\d+',row))) for row in re.findall(r'\{([\d,]+)\}',mapping)]
    assert len(rows)==4 and set(frame for row in rows for frame in row if frame!=255)==set(range(33))
    checked=[]
    with Image.open(root/'ruler-portraits.png') as deck:
        for scenario,who,name in ((1,13,'liubei'),(1,1,'caocao'),(2,5,'sunce'),(3,6,'sunquan'),(4,3,'menghuo')):
            count=m.call('sango_scenario_load', scenario, 0x81605000)
            m.write(module.STATE['g_scenario_id'],bytes([scenario]))
            position=m.read(0x81605000,count).index(who)
            h.pending.extend([key(0x6c)]*position)
            h.pending.append(key(0x1c))
            assert m.call('sango_menu_choose_ruler',count,instruction_limit=150000000)==who
            assert not h.pending
            screen=image(h.pixels())
            frame=rows[scenario-1][who]
            assert pixels565(screen.crop((370,66,442,138)))==pixels565(deck.crop((0,frame*72,72,(frame+1)*72)))
            screen.save(out/f'mips-art-ruler-{name}.png')
            checked.append([scenario,who,frame])

    # Older installations can still load the seven-image pack with fallback
    # portraits. A malformed new portrait slot must fail before changing assets.
    pack=(R/'dist/h1/sango.h1r').read_bytes()
    portrait_header=len(pack)-4-33*72*72*2-12
    assert struct.unpack_from('<III',pack,portrait_header)==(72,2376,342144)
    legacy=b'H1SGUI2\0'+struct.pack('<I',7)+pack[12:portrait_header]
    legacy+=struct.pack('<I',zlib.crc32(legacy))
    m.write(0x81400000,legacy)
    assert m.call('h1_ui_use_buffer',0x81400000,len(legacy),instruction_limit=500000000)==1
    malformed=bytearray(pack)
    struct.pack_into('<I',malformed,portrait_header,71)
    struct.pack_into('<I',malformed,len(pack)-4,zlib.crc32(malformed[:-4]))
    m.write(0x81400000,malformed)
    assert m.call('h1_ui_use_buffer',0x81400000,len(malformed),instruction_limit=500000000)==0
    return checked


if __name__=='__main__':
    report={'result':'passed','menu_states':4,'period_tiles':4,
            'portrait_samples':draw_tests(),'portrait_slots':33,
            'legacy_pack_accepted':True,'invalid_deck_rejected':True,
            'scope':'actual compiled MIPS menu and ruler rendering with modeled H1 services',
            'bda_sha256':hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
            'h1r_sha256':hashlib.sha256((R/'dist/h1/sango.h1r').read_bytes()).hexdigest()}
    (R/'reports/h1-landscape-art-mips.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report))
