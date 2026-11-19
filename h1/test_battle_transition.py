"""Run the compiled H1 amount dialogs and the first battle LCD presentation."""
import hashlib
import json
import struct

from PIL import Image, ImageChops

from test_mips import module
from test_popup import B, R, Harness, image, key


class FirstBattleWait(Exception):
    pass


def main():
    h = Harness(real_battle=True)
    m, s, state = h.m, h.s, module.STATE
    window = s['window']
    # Match the live setup: the engine canvas, LCD source and auxiliary canvas differ.
    m.word(state['g_draw_context'], window)
    m.word(state['g_game_context'], 0x81740000)
    m.word(s['sango_legacy_blob'] + 0x81c2b11c - 0x81c1fef0, 0x81780000)

    gui, lcd = 0x80500000, 0x82800000
    m.word(0x83c00004, gui)
    ticks = iter(range(100, 20000, 2))

    def get_lcd(args):
        m.write(args[0], struct.pack('<6H', 0, 480, 272, 1920, 0, 32))
        return lcd

    for offset, name, argc, callback in ((0x6d8, 'test_raw_tick', 0, lambda _: next(ticks)),
                                          (0x8f4, 'test_lcd', 2, get_lcd)):
        address = 0x80002000 + offset
        m.word(gui + offset, address)
        m.write(address, bytes.fromhex('0800e00300000000'))
        m.symbols[name] = address
        m.intercept(name, argc, callback)

    for resource in ('food', 'money', 'troop'):
        dialog = f'sango_ui_input_{resource}_amount'
        for event, expected in ((key(0x1c), 800), (key(1), 65535)):
            assert h.run(dialog, [event], 800, 3000) == expected
            assert m.read(s['paint_depth'], 4) == bytes(4), dialog
            assert m.read(state['g_draw_context'], 4) == struct.pack('<I', window), dialog
            assert h.pixels() == h.background, dialog

    m.word(state['g_viewport_tiles_address'], B + 2304)
    m.word(state['g_scratch_buffer_address'], 0x81609000)
    m.write(state['g_battle_control_mode'], b'\x01')
    m.write(state['g_battle_roster_first'], bytes([14]) + bytes(9))
    m.write(state['g_battle_roster_second'], bytes([1]) + bytes(9))
    for who in (0, 13):
        m.write(0x81601000 + who*16 + 10, struct.pack('<H', 800))
    m.write(state['g_battle_map_resource'], struct.pack('<H', 112))
    m.write(state['g_battle_attacker_food'], struct.pack('<H', 800))
    m.write(state['g_battle_defender_food'], struct.pack('<H', 3000))

    stop_at_battle = True

    def wait_for_input(_):
        if stop_at_battle:
            raise FirstBattleWait
        return 0

    m.intercept('sango_input_wait', 2, wait_for_input)
    try:
        m.call('sango_battle_run', instruction_limit=30_000_000)
    except FirstBattleWait:
        pass
    else:
        raise AssertionError('Battle ended before the first field redraw')

    assert m.read(s['h1_layout_game'], 4) == struct.pack('<I', 2)
    assert m.read(s['h1_native_active'], 4) == bytes(4)
    assert m.read(state['g_battle_map_width'], 1) == b'\x20'
    assert m.read(state['g_battle_map_height'], 1) == b'\x20'
    assert int.from_bytes(m.read(s['h1_diagnostic'] + 12, 4), 'little') > 0
    frame = Image.frombytes('RGB', (480, 272), m.read(lcd, 480*272*4), 'raw', 'BGRX')
    assert h.pixels() != h.background
    assert len(frame.crop((0, 0, 480, 216)).getcolors(65536)) > 100
    dest = R/'reports/screenshots/mips-battle-after-amount.png'
    frame.save(dest)

    stop_at_battle = False
    h.frames.clear()
    # Use the real weather/day sequence and the result entry called after a loss.
    m.call('sango_battle_advance_day', instruction_limit=30_000_000)
    m.call('sango_strategy_show_battle_defeat', instruction_limit=30_000_000)
    assert len(h.frames) == 6 and not h.allocations
    overlay_bounds = {}
    for index, name in enumerate(('weather', 'day', 'defeat')):
        shown = image(h.frames[2*index])
        restored = image(h.frames[2*index+1])
        bounds = ImageChops.difference(shown, restored).getbbox()
        assert bounds is not None, name
        left, top, right, bottom = bounds
        assert abs(left+right-480)<=1 and abs(top+bottom-216)<=1, (name, bounds)
        assert 0<top<bottom<216 and 40<right-left<300, (name, bounds)
        overlay_bounds[name] = bounds
        shown.save(R/'reports/screenshots'/f'mips-battle-notice-{name}.png')
    assert h.pixels() == h.frames[-1] and m.read(s['h1_native_active'], 4) == bytes(4)
    report = {'result': 'passed', 'scope': 'compiled MIPS with modeled firmware LCD, heap and input',
              'checks': ['food, money and troop dialogs confirm and cancel without an open paint scope',
                         'player sortie loads the 32x32 battle map and publishes the first real field frame',
                         'weather, day and defeat notices stay in the center of the battlefield'],
              'overlay_bounds': overlay_bounds,
              'screenshot': str(dest),
              'bda_sha256': hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest()}
    (R/'reports/h1-battle-transition-mips.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
