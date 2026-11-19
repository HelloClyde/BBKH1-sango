"""Drive real H1 list gestures from firmware mouse packets to rendered rows."""
from collections import deque
import hashlib
import json
import struct

from test_mips import module
from test_popup import B, Harness, R, image


def main():
    h = Harness(native_input=True)
    m = h.m
    requests = deque()
    sample = (0, 0, 0, 0, 0)
    m.word(0x83c00004, 0x80800000)

    def fetch(args):
        nonlocal sample
        assert requests, 'List was still waiting after the final input'
        sample = requests.popleft()
        m.word(args[0], sample[0] & 0xffffffff)
        m.word(args[1], sample[1])
        return 0

    def pen(args):
        for ptr, coord in zip(args, sample[3:]):
            m.write(ptr, struct.pack('<H', coord))
        return 0

    for i, (offset, nargs, handler) in enumerate(((0x750, 2, fetch), (0x6D8, 0, lambda _: sample[2]), (0x6C0, 2, pen))):
        name, address = f'list_native_{i}', 0x80002000+i*16
        m.word(0x80800000+offset, address)
        m.write(address, bytes.fromhex('0800e00300000000'))
        m.symbols[name] = address
        m.intercept(name, nargs, handler)

    tick = 100

    def enqueue(code, x=480, y=272, value=0, delay=3):
        nonlocal tick
        tick += delay
        requests.append((code, value, tick, x, y))

    def drag(x, start, middle, end):
        enqueue(11, x, start)
        enqueue(-1, x, start)
        enqueue(-1, x, middle)
        enqueue(8, x, end)

    def tap(x, y):
        enqueue(11, x, y)
        enqueue(8, x, y, delay=1)

    def press(code=23):
        enqueue(9, value=code)

    def clean():
        assert not requests
        assert m.read(h.s['h1_list_touch_active'], 4) == b'\0'*4

    def at(frame, x, y):
        return struct.unpack_from('<H', frame, 2*(y*480+x))[0]

    ids = 0x81605000
    count = m.call('sango_scenario_load', 1, ids)
    m.write(module.STATE['g_scenario_id'], b'\1')
    rulers = m.read(ids, count)
    out = R/'reports/screenshots'
    drag(240, 190, 157, 130)   # Starts over the map and must leave the list alone.
    drag(80, 190, 157, 130)    # Two native moves plus the partial release step.
    tap(65, 87)
    press()
    chosen = m.call('sango_menu_choose_ruler', count, instruction_limit=150000000)
    assert chosen == rulers[3], (chosen, list(rulers), len(h.frames),
                               [next(y for y in range(79, 219) if at(frame, 150, y) == 0x71A4)
                                for frame in h.frames])
    clean()
    assert at(h.frames[0], 150, 79) == 0x71A4 and at(h.frames[-1], 150, 79) == 0xC4AB
    image(h.frames[-1]).save(out/'mips-list-scroll-ruler.png')

    h.reset()
    drag(70, 190, 157, 130)
    drag(74, 107, 131, 167)
    press()
    assert m.call('sango_menu_choose_ruler', count, instruction_limit=150000000) == rulers[3]
    clean()
    # After a reverse drag, the thumb goes back to the top while the selected
    # ruler stays selected because it is still visible.
    assert at(h.frames[-1], 150, 79) == 0x71A4

    h.reset()
    press(24)
    assert m.call('sango_menu_choose_ruler', 6, instruction_limit=150000000) == 255
    clean()
    assert at(h.frames[0], 150, 79) not in (0x71A4, 0xC4AB)

    h.reset()
    drag(110, 187, 150, 126)
    tap(68, 81)
    tap(392, 225)
    assert h.run('sango_ui_select_character', [], ids, count, 0, 0, 0, 0) == 3
    clean()
    assert at(h.frames[0], 436, 72) == 0x71A4 and at(h.frames[-2], 436, 72) == 0xC4AB
    h.reset()
    press(24)
    assert h.run('sango_ui_select_character', [], ids, 6, 0, 0, 0, 0) == 255
    clean()
    assert at(h.frames[0], 436, 72) not in (0x71A4, 0xC4AB)

    h.reset()
    drag(176, 95, 63, 34)
    tap(176, 25)
    assert h.option(17, []) == 2
    clean()
    assert at(h.frames[0], 228, 15) == 0x71A4 and at(h.frames[-2], 228, 15) == 0xC4AB
    image(h.frames[-2]).save(out/'mips-list-scroll-popup.png')
    h.reset()
    press(24)
    assert h.option(7, []) == 255
    clean()
    assert at(h.frames[0], 228, 15) not in (0x71A4, 0xC4AB)

    def item_name(args):
        m.write(args[1], '铜剑'.encode('gbk')+b'\0')
    m.intercept('sango_item_read_name', 2, item_name)
    h.reset()
    m.write(B+2048, bytes(range(12)))
    drag(28, 140, 107, 75)
    press()
    assert h.run('sango_ui_select_item', [], B+2048, 12, 0, 0, 100, 100) == 3
    clean()

    m.intercept('bda_sango_fs_fopen', 2, lambda _: 0)
    saved_slots = []
    m.intercept('sango_save_write_slot', 1, lambda a: saved_slots.append(a[0]) or 1)
    h.reset()
    drag(100, 162, 121, 100)
    press()
    assert h.run('sango_save_choose_slot', [], 0) == 1 and saved_slots == [1]
    clean()

    report = {
        'result': 'passed', 'scope': 'Compiled MIPS from modeled native pen packets through list rendering',
        'lists': ['rulers', 'characters', 'commands', 'items', 'save slots'],
        'scrollbar': 'Only shows on overflow and moves with the viewport',
        'other_checks': ['reverse drag', 'off-list origin', 'tap following scroll', 'save only on confirmation'],
        'bda_sha256': hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
    }
    (R/'reports/h1-list-scroll-mips.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
