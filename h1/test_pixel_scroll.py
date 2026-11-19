"""Check actual H1 MIPS render and touch controllers with sub-tile drags."""
import hashlib
import json
import struct
from PIL import ImageChops
from test_popup import Harness, key, tap, image, R, B
from test_mips import module

def pan(dx, dy, end=False):
    return (13 if end else 12, (dx & 65535) | ((dy & 65535) << 16), 0)

def shift_matches(before, after, x, y, height):
    width = 480
    assert ImageChops.difference(before.crop((x,y,width,height)),
                                 after.crop((0,0,width-x,height-y))).getbbox() is None
    assert ImageChops.difference(before.crop((0,height,width,272)),
                                 after.crop((0,height,width,272))).getbbox() is None

def strategy():
    h = Harness(map_cache=True)
    h.m.intercept('sango_touch_started_in_map', 2, lambda a: 1)
    address = B + 1024
    h.m.write(address, bytes([0,0,7,1]))
    assert h.run('sango_strategy_select_city', [pan(0,-9),pan(0,-1),pan(0,0,True), key(1)], address) == 255
    assert len(h.frames) == 3, len(h.frames)
    initial, intermediate, final = map(image, h.frames)
    shift_matches(initial, intermediate, 0, 9, 240)
    shift_matches(intermediate, final, 0, 1, 240)
    # The partially exposed seventh row participates in hit testing.
    assert h.m.read(address, 4) == bytes([0,0,7,1])
    h.frames.clear()
    h.run('sango_strategy_select_city', [tap(479,239),key(1)], address)
    assert h.m.read(address+2,2) == bytes([11,6])
    h.frames.clear()
    h.run('sango_strategy_select_city', [pan(0,-480), pan(0,500,True), key(1)], address)
    assert h.m.read(address+1,1) == b'\0'
    return {'native_pixel_steps': [9,1], 'partial_row_hit': [11,6], 'retained_on_release': True}

def battle():
    h = Harness(real_battle=True)
    m, s, state = h.m, h.s, module.STATE
    m.word(state['g_viewport_tiles_address'], B+2304)
    m.word(state['g_game_context'], s['window'])
    m.word(state['g_draw_context'], s['window'])
    m.write(state['g_battle_map_resource'],struct.pack('<H',110))
    m.write(state['g_battle_tileset_resource'],struct.pack('<H',4))
    resource=m.call('sango_battle_resource_item',110,1)
    width,height=struct.unpack('<HH',m.read(resource,4))
    m.write(state['g_battle_map_width'],bytes([width]))
    m.write(state['g_battle_map_height'],bytes([height]))
    assert width>20 and height>9
    m.intercept('sango_touch_started_in_map',2,lambda a: 1)
    m.intercept('sango_battle_status_draw',0,lambda a: 0)
    m.intercept('sango_battle_draw_cursor_description',0,lambda a: 0)
    m.intercept('sango_battle_present',0,lambda a: 0)
    for i in range(20):
        m.write(state['g_first_unit_status']+7*i, b'\x08')
    m.write(state['g_battle_cursor_x'],b'\x07')
    m.write(state['g_battle_cursor_y'],b'\x04')
    h.reset(battle=True)
    m.call('sango_battle_draw_terrain',0,0,instruction_limit=30000000)
    def compose():
        m.call('h1_compose_battle',instruction_limit=30000000)
        return image(h.pixels())
    aligned = compose()
    # At an even camera position the old 3:2 expansion repeated each first
    # sample in a group of three. Native tiles retain detail between them.
    assert sum(aligned.getpixel((x,y)) != aligned.getpixel((x+1,y))
               for y in range(216) for x in range(0,480,3)) > 1000
    h.run('sango_battle_poll_map_action',[pan(-9,-9),pan(0,0,True),key(1)],0)
    # Remove the cursor so comparison covers the terrain itself.
    m.call('sango_battle_draw_tile',7,4,0)
    shifted = compose()
    shift_matches(aligned, shifted, 9, 9, 216)
    h.run('sango_battle_poll_map_action',[pan(-1,-1),pan(0,0,True),key(1)],0)
    m.call('sango_battle_draw_tile',7,4,0)
    sub_tile = compose()
    shift_matches(shifted, sub_tile, 1, 1, 216)
    assert h.run('sango_battle_poll_map_action',[tap(479,215)],0) == 0
    assert m.read(state['g_battle_cursor_x'],1) == bytes([20])
    assert m.read(state['g_battle_cursor_y'],1) == bytes([9])
    assert m.read(state['g_battle_viewport_x'],1) == b'\0'
    assert m.read(state['g_battle_viewport_y'],1) == b'\0'
    # The right/bottom strips must be the same map pixels seen when the next
    # complete tile becomes the viewport origin, not a blank or HUD strip.
    m.call('sango_battle_draw_terrain',1,1,instruction_limit=30000000)
    next_tile = compose()
    assert ImageChops.difference(sub_tile.crop((14,14,480,216)),
                                 next_tile.crop((0,0,466,202))).getbbox() is None
    h.run('sango_battle_poll_map_action',[pan(-1000,-1000),key(1)],0)
    assert m.read(state['g_battle_viewport_x'],1) == bytes([width-20])
    assert m.read(state['g_battle_viewport_y'],1) == bytes([height-9])
    h.run('sango_battle_poll_map_action',[pan(1000,1000,True),key(1)],0)
    assert m.read(state['g_battle_viewport_x'],1) == b'\0'
    assert m.read(state['g_battle_viewport_y'],1) == b'\0'
    return {'native_pixel_steps': [[9,9],[1,1]], 'partial_tile_hit': [20,9], 'world': [width,height],
            'extra_edge_pixels_match_next_tile': True, 'retained_on_release': True}

if __name__ == '__main__':
    report={'result':'passed','scope':'compiled MIPS controllers and pixel comparisons with modeled firmware services',
            'strategy':strategy(),'battle':battle(),
            'bda_sha256':hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest()}
    (R/'reports/h1-pixel-scroll-mips.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report))
