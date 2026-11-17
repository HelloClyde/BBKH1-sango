"""Render a legal battle fixture through the actual H1 MIPS renderer.
This is an offline MIPS render, explicitly not a firmware battle completion.
"""
import json,struct,hashlib
from pathlib import Path
from PIL import Image
import test_mips as base
R=base.ROOT;S=base.module.STATE;B=base.module.BUFFER
def main():
    m=base.H1Machine(base.ELF);sym=m.symbols
    lib=(R/'dist/h1/sango.lib').read_bytes();clr=(R/'dist/h1/sango.clr').read_bytes();art=(R/'dist/h1/sango.h1r').read_bytes()
    m.write(0x81000000,lib);m.word(S['g_resource_package_address'],0x81000000)
    m.write(0x81100000,clr);assert m.call('sango_color_use_buffer',0x81100000,len(clr),len(lib),instruction_limit=300000000)==1
    m.write(0x81400000,art);assert m.call('h1_ui_use_buffer',0x81400000,len(art),instruction_limit=500000000)==1
    for name,address in [('g_character_table_address',0x81601000),('g_viewport_tiles_address',0x81600000),('g_move_costs_address',0x81604000),('g_temp_character_ids_address',0x81605000),('g_army_slot_used_address',0x81606000),('g_army_rosters_address',0x81607000),('g_order_queue_address',0x81608000),('g_scratch_buffer_address',0x81609000)]:m.word(S[name],address)
    m.word(S['g_draw_context'],sym['window']);m.word(S['g_game_context'],sym['window'])
    m.intercept('bda_sango_gui_draw_guard',1,lambda a:1);m.intercept('bda_sango_gui_request_redraw',3,lambda a:1);m.intercept('sango_battle_present',0,lambda a:0)
    m.call('sango_scenario_load',1,0x81605000)
    m.write(S['g_player_character'],b'\x0d');m.write(S['g_battle_control_mode'],b'\x01')
    m.write(S['g_battle_roster_first'],bytes([14])+bytes(9));m.write(S['g_battle_roster_second'],bytes([1])+bytes(9))
    for who in (0,13):m.write(0x81601000+who*16+10,struct.pack('<H',800))
    m.write(S['g_battle_map_resource'],struct.pack('<H',110));m.write(S['g_battle_tileset_resource'],struct.pack('<H',4));m.write(S['g_battle_attacker_food'],struct.pack('<H',3000));m.write(S['g_battle_defender_food'],struct.pack('<H',3000));m.write(S['g_battle_player_unit_begin'],b'\0')
    m.call('sango_battle_initialize_unit_stats');m.call('sango_battle_initialize_field');m.call('sango_battle_redraw_field',0,instruction_limit=20000000)
    m.call('h1_compose_battle',instruction_limit=20000000)
    raw=m.read(sym['h1_native_pixels'],480*272*2);pixels=struct.unpack('<'+'H'*(480*272),raw)
    assert len(set(pixels))>200
    image=Image.new('RGB',(480,272));image.putdata([((p>>11)*255//31,((p>>5)&63)*255//63,(p&31)*255//31) for p in pixels]);dest=R/'reports/screenshots/mips-native-battle.png';image.save(dest)
    report={'result':'passed','bda_sha256':hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),'type':'offline compiled-MIPS battle fixture','world':[m.read(S['g_battle_map_width'],1)[0],m.read(S['g_battle_map_height'],1)[0]],'display_tiles':[20,9],'framebuffer':[480,272],'screenshot':str(dest),'complete_battle_verified':False}
    (R/'reports/h1-landscape-battle-render.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report))
if __name__=='__main__':main()
