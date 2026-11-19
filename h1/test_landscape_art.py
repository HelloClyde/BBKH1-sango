"""Exercise the new H1R deck through the real compiled menu and ruler pages."""
import hashlib
import json
import re
import struct
import zlib

from test_popup import B, Harness, R, image, key, tap
from test_mips import UI_ADDRESS, module
from bitmap_font import TOP_PAD, text_mask


def pixels565(picture):
    return b''.join(struct.pack('<H', ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
                    for r, g, b in picture.convert('RGB').getdata())


def draw_tests():
    h = Harness()
    m = h.m
    out = R / 'reports/screenshots'
    root = R / 'assets/h1-landscape/native'
    from PIL import Image
    with Image.open(root/'panel.png') as backdrop:
        corners = [(rect, pixels565(backdrop.crop(rect))) for rect in
                   ((0, 0, 42, 69), (438, 0, 480, 69), (0, 222, 42, 272), (438, 222, 480, 272))]
    with Image.open(root/'credits-landscape.png') as artwork:
        credits = pixels565(artwork)

    def check_corners(screen):
        for rect, reference in corners:
            assert pixels565(screen.crop(rect)) == reference, rect

    title = None
    for selected in range(4):
        m.call('h1_draw_main', selected)
        screen = image(h.pixels())
        with Image.open(root / f'main-{selected}.png') as preview:
            assert h.pixels() == pixels565(preview)
        for y, label in zip((70,111,152,193),('新君登陆','重返沙场','游戏设置','卸甲归田')):
            mask = text_mask(label,24)
            x, top = 250+(204-mask.width)//2, y+5-TOP_PAD
            assert all(screen.getpixel((x+dx,top+dy))==(40,24,8)
                       for dy in range(mask.height) for dx in range(mask.width) if mask.getpixel((dx,dy)))
        current = screen.crop((0, 0, 480, 48)).tobytes()
        if title is None:
            title = current
            screen.save(out / 'mips-art-main.png')
        else:
            assert title == current

    for index, x in enumerate((88, 189, 290, 391)):
        h.pending.extend([tap(24, 58), tap(139, 80), tap(x, 80)])
        assert m.call('sango_menu_select_animated',45,0,instruction_limit=150000000)==index
        assert not h.pending
    h.pending.extend([key(0x6a), key(0x1c)])
    assert m.call('sango_menu_select_animated',45,0,instruction_limit=150000000)==1
    screen=image(h.pixels())
    check_corners(screen)
    with Image.open(root/'scenarios.png') as expected:
        for index in range(4):
            x, source_x = 46 + 101*index, 23 + 116*index
            assert pixels565(screen.crop((x,56,x+85,143))) == pixels565(expected.crop((source_x,56,source_x+85,143)))
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
            check_corners(screen)
            frame=rows[scenario-1][who]
            assert pixels565(screen.crop((360,66,432,138)))==pixels565(deck.crop((0,frame*72,72,(frame+1)*72)))
            screen.save(out/f'mips-art-ruler-{name}.png')
            checked.append([scenario,who,frame])

    count = m.call('sango_scenario_load', 1, 0x81605000)
    m.write(module.STATE['g_scenario_id'], b'\x01')
    rulers = m.read(0x81605000, count)
    h.pending.extend([tap(25, 103), tap(62, 110), tap(451, 224), tap(383, 222)])
    assert m.call('sango_menu_choose_ruler',count,instruction_limit=150000000)==rulers[1] and not h.pending
    h.pending.append(tap(290, 222))
    assert m.call('sango_menu_choose_ruler',count,instruction_limit=150000000)==255 and not h.pending

    def page_frames(function, events, *args):
        h.reset()
        result = h.run(function, events, *args)
        screens = [image(frame) for frame in h.frames[:-1]]
        assert screens
        for frame,current in zip(h.frames,screens):
            if frame!=credits:
                check_corners(current)
        return result, screens[0]

    m.intercept('sango_settings_sdk_write', 2, lambda args: 8)
    page_frames('sango_settings_show', [tap(52, 184), tap(236, 251), tap(425, 184)])
    assert h.frames.count(credits)==1
    image(credits).save(out/'mips-art-credits.png')
    result, _ = page_frames('sango_ui_select_character', [tap(20, 195), tap(100, 195), tap(390, 225)], 0x81605000, count, 0, 0, 0, 0)
    assert result == 5 and len(h.frames)==4
    image(h.frames[-2]).save(out/'mips-art-select-character.png')
    result, _ = page_frames('sango_ui_select_character', [tap(100, 95), tap(289, 225)], 0x81605000, count, 0, 0, 0, 0)
    assert result == 255 and len(h.frames)==3
    result, _ = page_frames('sango_ui_select_number', [tap(450, 220), tap(378, 163), tap(385, 220)], 5, 100)
    assert result == 6
    m.intercept('bda_sango_fs_fopen', 2, lambda args: 0)
    slots = []
    m.intercept('sango_save_load_slot', 1, lambda args: slots.append(args[0]) or 1)
    m.intercept('sango_save_write_slot', 1, lambda args: slots.append(args[0]) or 1)
    result, screen = page_frames('sango_save_choose_slot', [tap(421, 230)], 0)
    assert result == 0
    screen.save(out/'mips-frame-save-slots.png')
    result, _ = page_frames('sango_save_choose_slot', [tap(20, 71), tap(87, 105), tap(55, 125)], 1)
    assert result == 1 and slots == [0, 1]
    page_frames('sango_ui_show_city_details', [key(1)], 0)

    h.reset()
    m.write(B+1024, bytes([0, 0, 7, 1]))
    assert h.run('sango_strategy_select_city', [tap(380, 255), key(0x1c), key(1)], B+1024) == 255
    check_corners(image(h.frames[1]))

    state = module.STATE
    m.write(state['g_battle_map_resource'], struct.pack('<H', 110))
    m.write(state['g_battle_tileset_resource'], struct.pack('<H', 4))
    resource = m.call('sango_battle_resource_item', 110, 1)
    width, height = struct.unpack('<HH', m.read(resource, 4))
    m.write(state['g_battle_map_width'], bytes([width]))
    m.write(state['g_battle_map_height'], bytes([height]))
    for unit in range(20):
        m.write(state['g_first_unit_status']+7*unit, b'\x08')
    m.write(state['g_battle_cursor_x'], b'\x07')
    m.write(state['g_battle_cursor_y'], b'\x04')
    page_frames('sango_battle_info_show', [key(1)])
    page_frames('sango_battle_show_overview', [key(1)])

    # Older installations can still load the seven-image pack with fallback
    # portraits. A malformed new portrait slot must fail before changing assets.
    pack=(R/'dist/h1/sango.h1r').read_bytes()
    portrait_header=12
    for _ in range(7):
        portrait_header+=12+struct.unpack_from('<I',pack,portrait_header+8)[0]
    assert struct.unpack_from('<III',pack,portrait_header)==(72,2376,342144)
    legacy=b'H1SGUI2\0'+struct.pack('<I',7)+pack[12:portrait_header]
    legacy+=struct.pack('<I',zlib.crc32(legacy))
    m.write(UI_ADDRESS,legacy)
    assert m.call('h1_ui_use_buffer',UI_ADDRESS,len(legacy),instruction_limit=500000000)==1
    h.pending.append(tap(391, 80))
    assert m.call('sango_menu_select_animated',45,0,instruction_limit=150000000)==3 and not h.pending
    check_corners(image(h.pixels()))
    malformed=bytearray(pack)
    struct.pack_into('<I',malformed,portrait_header,71)
    struct.pack_into('<I',malformed,len(pack)-4,zlib.crc32(malformed[:-4]))
    m.write(UI_ADDRESS,malformed)
    assert m.call('h1_ui_use_buffer',UI_ADDRESS,len(malformed),instruction_limit=500000000)==0
    return checked


if __name__=='__main__':
    report={'result':'passed','menu_states':4,'period_tiles':4,
            'main_menu_ma_shan_zheng_pixels':24,'credits_image_from_settings':True,
            'portrait_samples':draw_tests(),'portrait_slots':33,
            'legacy_pack_accepted':True,'invalid_deck_rejected':True,
            'frame_corner_checks':['periods','rulers','settings','credits','character list',
                                   'number entry','save','load','city',
                                   'faction overview','battle terrain information','battle overview'],
            'inset_touch_checks':['all four period cards','ruler list and actions','character list',
                                 'number confirmation','save and load slots'],
            'scope':'actual compiled MIPS menu and ruler rendering with modeled H1 services',
            'bda_sha256':hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
            'h1r_sha256':hashlib.sha256((R/'dist/h1/sango.h1r').read_bytes()).hexdigest()}
    (R/'reports/h1-landscape-art-mips.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report))
