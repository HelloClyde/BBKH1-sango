"""Execute actual MIPS popups; model only native heap, display and input."""
from collections import deque
import hashlib
import json
import struct
from PIL import Image, ImageChops
import test_mips as base

R = base.ROOT
SIZE = 480 * 272 * 2
B = base.module.BUFFER

def key(value): return (5, value, 0)
def tap(x, y): return (11, 0, x | y << 16)
def image(raw):
    im = Image.new('RGB', (480, 272))
    im.putdata([(((p >> 11) & 31) << 3, ((p >> 5) & 63) << 2, (p & 31) << 3)
                for p in struct.unpack('<' + 'H' * (480 * 272), raw)])
    return im

class Harness:
    def __init__(self, *, map_cache=False, real_battle=False):
        self.m = m = base.H1Machine(base.ELF)
        self.s = m.symbols
        self.pending = deque()
        self.frames = []
        self.allocations = {}
        self.fail_alloc = False
        self.map_cache = map_cache
        self.battle_compositions = 0
        self.battle_frame = b'\x34\x12' * (480 * 272)
        lib = (R / 'dist/h1/sango.lib').read_bytes()
        art = (R / 'dist/h1/sango.h1r').read_bytes()
        clr = (R / 'dist/h1/sango.clr').read_bytes()
        m.write(0x81000000, lib)
        m.word(base.module.STATE['g_resource_package_address'], 0x81000000)
        m.write(0x81400000, art)
        assert m.call('h1_ui_use_buffer', 0x81400000, len(art), instruction_limit=500000000) == 1
        m.write(0x81100000, clr)
        assert m.call('sango_color_use_buffer', 0x81100000, len(clr), len(lib), instruction_limit=300000000) == 1
        for name, address in [('g_character_table_address', 0x81601000), ('g_temp_character_ids_address', 0x81605000),
                              ('g_army_slot_used_address', 0x81606000), ('g_army_rosters_address', 0x81607000),
                              ('g_order_queue_address', 0x81608000)]:
            m.word(base.module.STATE[name], address)
        m.call('sango_scenario_load', 1, 0x81605000)
        m.write(base.module.STATE['g_player_character'], b'\x0d')
        m.intercept('h1_ui_assets_load', 0, lambda a: 1)
        m.intercept('h1_native_flip', 0, lambda a: self.frames.append(self.pixels()))
        def event(args):
            assert self.pending, 'Unexpected extra input wait'
            m.write(args[0], struct.pack('<3I', *self.pending.popleft()))
            return 0
        m.intercept('sango_input_next_event', 1, event)
        m.word(0x83c00010, 0x80800000)
        for i, (offset, fn) in enumerate([(8, self.allocate), (12, self.free)]):
            name = 'popup_heap_' + str(i)
            pc = 0x80001000 + i * 16
            m.word(0x80800000 + offset, pc)
            m.write(pc, bytes.fromhex('0800e00300000000'))
            m.symbols[name] = pc
            m.intercept(name, 1, fn)
        def compose(args):
            self.battle_compositions += 1
            m.write(self.s['h1_native_pixels'], self.battle_frame)
            return 0
        if not real_battle:
            m.intercept('h1_compose_battle', 0, compose)
        m.write(B + 1024, bytes([0, 0, 7, 1]))
        m.call('sango_strategy_draw_map', B + 1024, 0, instruction_limit=30000000)
        self.background = self.pixels()
        self.reset()

    def allocate(self, args):
        # The persistent map cache is optional and exercised by the map test.
        if args[0] == 480 * 360 * 2 and self.map_cache: return 0x81900000
        if self.fail_alloc or args[0] > 0x40000: return 0
        address = 0x81800000
        while address in self.allocations: address += 0x40000
        assert args[0] <= 0x40000
        self.allocations[address] = args[0]
        return address

    def free(self, args):
        assert args[0] in self.allocations, ('unknown/double free', args[0])
        del self.allocations[args[0]]
        return 0

    def pixels(self): return self.m.read(self.s['h1_native_pixels'], SIZE)

    def reset(self, battle=False):
        assert not self.allocations
        m = self.m
        m.write(self.s['h1_native_pixels'], self.background)
        m.word(self.s['h1_native_active'], 0 if battle else 1)
        m.word(self.s['h1_layout_game'], 2 if battle else 1)
        m.word(self.s['h1_screen_id'], 13)
        self.frames.clear()
        self.pending.clear()

    def run(self, function, events, *args):
        self.pending.extend(events)
        result = self.m.call(function, *args, instruction_limit=150000000)
        assert not self.pending
        assert not self.allocations, self.allocations
        assert int.from_bytes(self.m.read(self.s['h1_screen_id'], 4), 'little') == 13
        return result

    def option(self, count, events, initial=0, bounds=(79, 10, 103, 190), text=None):
        self.m.write(B, struct.pack('<4H', *bounds))
        stride = (bounds[2] - bounds[0]) // 6
        rows = text or b''.join(('命令'.encode('gbk') + bytes([65 + i % 26]) + b' ' * stride)[:stride] for i in range(count))
        self.m.write(B + 32, rows + b'\0')
        return self.run('sango_ui_choose_option', events, B, initial, B + 32)

def main():
    h = Harness()
    checks = []
    screenshots = R / 'reports/screenshots'
    screenshots.mkdir(exist_ok=True)
    assert h.run('sango_ui_show_system_menu', [key(1)]) == 0
    popup = h.frames[0]
    bbox = ImageChops.difference(image(popup), image(h.background)).getbbox()
    assert bbox == (152, 7, 235, 100), bbox
    assert h.pixels() == h.background
    image(popup).save(screenshots / 'mips-popup-system.png')
    checks.append('system popup is only 83x93 including shadow; map/HUD unchanged outside it')
    for count, events, wanted, initial in [(4, [key(0x6c), key(0x1c)], 1, 0),
                                         (17, [key(0x67), key(0x1c)], 16, 0),
                                         (17, [key(0x6c), key(0x1c)], 7, 6),
                                         (4, [tap(180, 104)], 3, 0),
                                         (4, [tap(450, 230)], 255, 0),
                                         (4, [tap(152, 7), key(1)], 255, 0),
                                         (4, [key(0x1c)], 0, 99),
                                         (0, [], 255, 0)]:
        h.reset()
        assert h.option(count, events, initial) == wanted
        assert h.pixels() == h.background
        checks.append('keyboard/touch/scroll/empty/initial selection case ' + str(len(checks)))
    for group, wanted in [(0, 1), (1, 15), (2, 23), (3, 254)]:
        h.reset()
        events = [key(0x6c)] * group + [key(0x1c)]
        if group != 3: events.append(key(0x1c))
        assert h.run('sango_city_choose_command', events) == wanted
        assert h.pixels() == h.background
        checks.append('original city command mapping group ' + str(group))
    h.reset()
    assert h.run('sango_city_choose_command', [key(0x1c), key(1), key(1)]) == 255
    root = h.frames[1]
    child = next(frame for frame in h.frames if ImageChops.difference(image(frame), image(h.background)).getbbox() == (152, 7, 319, 208))
    for y in range(7, 123):
        assert child[(y * 480 + 152) * 2:(y * 480 + 232) * 2] == root[(y * 480 + 152) * 2:(y * 480 + 232) * 2]
    assert h.pixels() == h.background
    image(child).save(screenshots / 'mips-popup-cascade.png')
    checks.append('domestic submenu retains the selected parent; nested cancel restores the original map')
    h.reset()
    assert h.run('sango_city_choose_command', [tap(180, 26), tap(450, 230), tap(450, 230)]) == 255
    assert h.pixels() == h.background
    checks.append('outside touches dismiss child then parent without reaching the map')
    h.reset()
    assert h.option(40, [key(0x1c)], 39, (230, 220, 470, 500), ('很长的中文菜单'.encode('gbk') * 4)[:40] * 40) == 39
    clipped = ImageChops.difference(image(h.frames[0]), image(h.background)).getbbox()
    assert clipped == (244, 38, 475, 235), clipped
    assert h.pixels() == h.background
    checks.append('40 long GBK labels clip inside a bounded scrolled popup at the bottom/right edge')
    h.reset()
    def item_name(args): h.m.write(args[1], '铜剑'.encode('gbk') + b'\0'); return 0
    h.m.intercept('sango_item_read_name', 2, item_name)
    h.m.write(B + 2048, bytes([1, 2, 3, 4]))
    assert h.run('sango_ui_select_item', [key(0x6c), key(0x1c)], B + 2048, 4, 0, 0, 100, 100) == 1
    assert h.pixels() == h.background
    checks.append('item picker uses the same small popup and preserves item index semantics')
    h.reset(battle=True)
    assert h.option(3, [key(1)]) == 255
    assert h.battle_compositions == 1 and h.pixels() == h.battle_frame
    assert int.from_bytes(h.m.read(h.s['h1_native_active'], 4), 'little') == 0
    checks.append('fresh battle composition captured before popup; inactive battle mode restored')
    h.reset()
    h.fail_alloc = True
    assert h.option(3, []) == 255 and h.pixels() == h.background
    checks.append('snapshot allocation failure cancels without erasing the screen')
    report = {'result': 'passed', 'checks': checks, 'bda_sha256': hashlib.sha256((R / 'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
              'scope': 'Actual compiled MIPS popup/controller/render logic; native heap/input/display modeled. Not physical-device evidence.'}
    (R / 'reports/h1-popup-mips.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__': main()
