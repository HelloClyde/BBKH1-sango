"""Verify the generated strategy assets in the compiled H1 map renderer."""
import hashlib
import json
import struct
import zlib

from PIL import Image
from test_mips import UI_ADDRESS, module
from test_popup import B, R, Harness, image, key, tap
from test_pixel_scroll import pan, shift_matches


def rgb565(rgb):
    r, g, b = rgb[:3]
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def blend(fg, bg, a):
    result = 0
    for shift, bits in ((0, 31), (5, 63), (11, 31)):
        component = (((fg >> shift & bits)*a + (bg >> shift & bits)*(255-a)+127)//255)
        result |= component << shift
    return result


def main():
    h = Harness()
    m = h.m
    root = R/'assets/h1-landscape/native'
    out = R/'reports/screenshots'
    with Image.open(root/'strategy-world.png') as im:
        world = im.convert('RGB')
    with Image.open(root/'strategy-banner.png') as im:
        strip = im.convert('RGBA')
    assert world.size == (480, 360) and strip.size == (8, 28)
    other, player = strip.crop((0,0,8,14)), strip.crop((0,14,8,28))
    assert sum(a>=128 and r>g+30 and r>b+40 for r,g,b,a in other.getdata()) >= 8
    assert sum(a>=128 and b>g+20 and b>r+40 for r,g,b,a in player.getdata()) >= 8

    grid_address = m.call('sango_resource_item', 2, 14, 0x81000000)
    cities = m.read(grid_address, 108)
    assert sorted(city for city in cities if city) == list(range(1, 39))
    state = B+1024
    m.write(state, bytes([0, 0, 7, 1]))
    m.call('sango_strategy_draw_map', state, 0, instruction_limit=30000000)
    top = h.pixels()
    image(top).save(out/'mips-strategy-hd-north.png')
    assert m.read(h.s['strategy_terrain'], 4) == bytes(4), 'HD map should not allocate a second terrain copy'

    def pixel(frame, x, y):
        return struct.unpack_from('<H', frame, 2*(y*480+x))[0]

    def nameplate(index, camera):
        m.call('sango_city_read_name', cities[index]-1, B+1056)
        name = m.read(B+1056, 32).split(b'\0', 1)[0]
        width = len(name)*6+10
        left = min(max(1,40*(index%12)+(40-width)//2),479-width)
        tile_y = 40*(index//12-camera)
        y = min(tile_y+29,224)
        return left,y,left+width,y+15

    alpha_checks = set()
    owner_base = module.STATE['g_first_city_owner']

    def check_tile(frame, index, camera, label=None):
        left, y = 40*(index%12), 40*(index//12)
        city = cities[index]
        owner = m.read(owner_base+(city-1)*36,1)[0] if city else 0
        banner = player if owner==14 else other if owner else None
        for dy in range(40):
            for dx in range(40):
                screen_y = y+dy-camera*40
                if label and label[0]<=left+dx<label[2] and label[1]<=screen_y<label[3]:
                    continue
                expected = rgb565(world.getpixel((left+dx,y+dy)))
                if banner and 27<=dx<35 and 4<=dy<18:
                    rgba = banner.getpixel((dx-27,dy-4))
                    expected = blend(rgb565(rgba),expected,rgba[3])
                    alpha_checks.add('transparent' if rgba[3]==0 else 'opaque' if rgba[3]==255 else 'edge')
                assert pixel(frame,left+dx,screen_y) == expected, (index,dx,dy)

    north_label = nameplate(19,0)
    for index in range(72):
        check_tile(top,index,0,north_label)
    assert alpha_checks == {'opaque','transparent','edge'}

    index = next(i for i,c in enumerate(cities[:72]) if c and i!=19)
    city = cities[index]
    x,y = 40*(index%12),40*(index//12)
    owner = owner_base+(city-1)*36
    for value in (0,1,14):
        m.write(owner,bytes([value]))
        m.call('sango_strategy_draw_map', state, 0, instruction_limit=30000000)
        check_tile(h.pixels(),index,0)

    h.frames.clear()
    m.intercept('sango_touch_started_in_map', 2, lambda _: 1)
    assert h.run('sango_strategy_select_city',[pan(0,-9),pan(0,-1),pan(0,0,True),key(1)],state) == 255
    assert len(h.frames)==3
    shift_matches(image(h.frames[0]),image(h.frames[1]),0,9,240)
    shift_matches(image(h.frames[1]),image(h.frames[2]),0,1,240)

    southernmost = max(i for i,city in enumerate(cities) if city)
    bottom_camera = southernmost//12-5
    m.write(state,bytes([0,bottom_camera,southernmost%12,southernmost//12]))
    m.call('sango_strategy_draw_map', state, 0, instruction_limit=30000000)
    south = h.pixels()
    image(south).save(out/'mips-strategy-hd-south.png')
    south_label = nameplate(southernmost,bottom_camera)
    assert south_label[3]==239, south_label
    for index in range(12*bottom_camera,12*(bottom_camera+6)):
        check_tile(south,index,bottom_camera,south_label)
    for index, city in enumerate(cities):
        if not city:
            continue
        col, row = index%12, index//12
        camera = 0 if row<6 else 3
        m.write(state,bytes([0,camera,7,camera+1]))
        assert h.run('sango_strategy_select_city',[tap(col*40+20,(row-camera)*40+20)],state) == city-1

    def check_focus(index, camera):
        col, row = index%12, index//12
        x, y = col*40, (row-camera)*40

        def draw(column):
            m.write(state,bytes([0,camera,column,row]))
            m.call('sango_strategy_draw_map',state,0,instruction_limit=30000000)
            return h.pixels()

        unselected = draw(col+1 if col==0 else col-1)
        selected = draw(col)
        if cities[index]:
            left,top,right,bottom = nameplate(index,camera)
            label = [pixel(selected,xx,yy) for yy in range(top,bottom) for xx in range(left,right)]
            assert label.count(0xff97)>25, 'city lettering should be clearly visible'
            assert all(color in label for color in (0x38c2,0x71a4,0x5143))
            assert all(pixel(selected,x+dx,y+dy)==pixel(unselected,x+dx,y+dy)
                       for dy in range(4,18) for dx in range(27,35))
        else:
            changed = [(dx,dy) for dy in range(40) for dx in range(40)
                       if pixel(unselected,x+dx,y+dy)!=pixel(selected,x+dx,y+dy)]
            assert len(changed)>90 and all(2<=dx<=38 and 14<=dy<=26 for dx,dy in changed)

    check_focus(19,0)  # 城池
    check_focus(southernmost,bottom_camera) # 底边城池
    check_focus(0,0)   # 西北山地
    check_focus(107,3) # 东南水面

    pack = (R/'dist/h1/sango.h1r').read_bytes()
    offset = 12
    for _ in range(8):
        offset += 12 + struct.unpack_from('<I',pack,offset+8)[0]
    # The previous versions still render their cities and ownership markers.
    sprite_offset = offset + 12 + struct.unpack_from('<I',pack,offset+8)[0]
    city_rgb, city_alpha = struct.pack('<H',0xe631)*1024, b'\xff'*1024
    v4 = (b'H1SGUI4\0'+struct.pack('<I',10)+pack[12:sprite_offset]
          +struct.pack('<III',32,32,3072)+city_rgb+city_alpha)
    v4 += struct.pack('<I',zlib.crc32(v4))
    m.write(UI_ADDRESS,v4)
    assert m.call('h1_ui_use_buffer',UI_ADDRESS,len(v4),instruction_limit=500000000)==1
    m.write(state,bytes([0,0,7,1]))
    m.call('sango_strategy_draw_map',state,0,instruction_limit=30000000)
    assert pixel(h.pixels(),x+27,y+6)==0xFEC4

    v5 = (b'H1SGUI5\0'+struct.pack('<I',10)+pack[12:sprite_offset]
          +struct.pack('<III',32,96,9216)+city_rgb*3+city_alpha*3)
    v5 += struct.pack('<I',zlib.crc32(v5))
    m.write(UI_ADDRESS,v5)
    assert m.call('h1_ui_use_buffer',UI_ADDRESS,len(v5),instruction_limit=500000000)==1
    m.call('sango_strategy_draw_map',state,0,instruction_limit=30000000)
    assert pixel(h.pixels(),x+27,y+6)==0xe631

    legacy = b'H1SGUI3\0'+struct.pack('<I',8)+pack[12:offset]
    legacy += struct.pack('<I',zlib.crc32(legacy))
    m.write(UI_ADDRESS,legacy)
    assert m.call('h1_ui_use_buffer',UI_ADDRESS,len(legacy),instruction_limit=500000000)==1
    assert m.read(h.s['strategy_world'],4)==bytes(4) and m.read(h.s['strategy_city'],4)==bytes(4)
    m.write(state,bytes([0,0,7,1]))
    m.call('sango_strategy_draw_map',state,0,instruction_limit=30000000)
    assert h.pixels()!=top

    report = {'result':'passed','scope':'compiled H1 MIPS renderer with modeled firmware services',
              'map_size':world.size,'cities_baked_into_map':True,'city_hit_tests':38,'banner_size':player.size,
              'ownership_banners':{'vacant':None,'other':'red','player':'blue'},
              'alpha_checks':sorted(alpha_checks),'vertical_pan_pixels':[9,1],
              'focus_marker':['city nameplate below and at bottom edge','mountain','water'],
              'legacy_versions':[3,4,5],'hd_terrain_cache_allocated':False,
              'bda_sha256':hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
              'h1r_sha256':hashlib.sha256(pack).hexdigest()}
    (R/'reports/h1-strategy-hd-mips.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
