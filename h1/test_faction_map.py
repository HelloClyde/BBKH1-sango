"""Verify the compiled H1 faction maps against the city grid and live city owners."""
import hashlib
import json
import struct

from test_popup import B, Harness, R, image, key, tap
from test_mips import module


def check_map(frame, cities, owners, selected, x, y, cell):
    colors = {0: 0x7BCE, selected: 0xFF06}
    expected_selected = 0
    for index, city in enumerate(cities):
        if city:
            px, py = x + index % 12 * cell + cell // 2, y + index // 12 * cell + cell // 2
            owner = owners[city - 1]
            assert struct.unpack_from('<H', frame, 2 * (py * 480 + px))[0] == colors.get(owner, 0xA925)
            expected_selected += owner == selected
    return expected_selected


def main():
    h = Harness()
    m = h.m
    grid_address = m.call('sango_resource_item', 2, 14, 0x81000000)
    cities = m.read(grid_address, 12 * 9)
    assert sorted(city for city in cities if city) == list(range(1, 39))
    owner_base = module.STATE['g_first_city_owner']
    out = R / 'reports/screenshots'
    previews = []

    def city_owners():
        return m.read(owner_base, 38 * 36)[::36]

    for scenario in range(1, 5):
        count = m.call('sango_scenario_load', scenario, 0x81605000)
        m.write(module.STATE['g_scenario_id'], bytes([scenario]))
        rulers = m.read(0x81605000, count)
        owners = city_owners()
        last_preview = rulers.index(13) if scenario == 1 else 1
        h.frames.clear()
        h.pending.extend([key(0x6C)] * last_preview + [key(0x1C)])
        assert m.call('sango_menu_choose_ruler', count, instruction_limit=150000000) == rulers[last_preview]
        assert len(h.frames) == last_preview + 1 and not h.pending
        for ruler, frame in zip(rulers, h.frames):
            owned_count = check_map(frame, cities, owners, ruler + 1, 162, 69, 15)
            assert owned_count > 0
            previews.append([scenario, ruler, owned_count])
        if scenario == 1:
            image(h.frames[-1]).save(out / 'mips-faction-ruler.png')

    m.call('sango_scenario_load', 1, 0x81605000)
    m.write(module.STATE['g_scenario_id'], b'\x01')
    player = 14
    m.write(module.STATE['g_player_character'], bytes([player - 1]))
    prior_owner = city_owners()
    city = next(index for index, owner in enumerate(prior_owner) if owner and owner != player)
    state = B + 1024
    m.write(state, bytes([0, 0, 7, 1]))

    def overview():
        h.frames.clear()
        assert h.run('sango_strategy_select_city', [tap(379, 255), key(0x1C), key(1)], state) == 255
        assert len(h.frames) == 3 and h.frames[0] == h.frames[2] == h.pixels()
        return h.frames[1]

    before = overview()
    before_count = check_map(before, cities, prior_owner, player, 120, 45, 20)
    image(before).save(out / 'mips-faction-overview.png')
    m.write(owner_base + city * 36, bytes([player]))
    after = overview()
    assert check_map(after, cities, city_owners(), player, 120, 45, 20) == before_count + 1
    changed = [index for index in range(480*272)
               if before[2*index:2*index+2] != after[2*index:2*index+2]]
    cell_index = cities.index(city + 1)
    px, py = 130 + (cell_index % 12) * 20, 55 + (cell_index // 12) * 20
    assert changed and all(abs(i % 480 - px) <= 4 and abs(i // 480 - py) <= 4 for i in changed)

    report = {
        'result': 'passed', 'scope': 'compiled MIPS with modeled H1 services',
        'ruler_previews': previews, 'city_markers': 38,
        'overview_restored_strategy_screen': True, 'overview_reflected_owner_change': True,
        'bda_sha256': hashlib.sha256((R / 'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),
    }
    (R / 'reports/h1-faction-map-mips.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
