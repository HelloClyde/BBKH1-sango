"""Targeted compiled-MIPS acceptance for the native landscape asset/layout revision."""
import hashlib,json,struct,types
from pathlib import Path
import test_mips as base
from PIL import Image
ROOT=base.ROOT;ELF=base.ELF;STATE=base.module.STATE;BUFFER=base.module.BUFFER
def check_native_contacts():
    m=base.H1Machine(ELF);state={'code':-1,'tick':100,'point':(240,84)}
    m.word(0x83c00004,0x80800000);m.word(m.symbols['h1_native_active'],1)
    m.word(m.symbols['h1_screen_id'],13)
    def event(args):
        m.word(args[0],state['code']);m.word(args[1],0xffffffff);state['code']=-1;return 0
    def pen(args):
        m.write(args[0],struct.pack('<H',state['point'][0]));m.write(args[1],struct.pack('<H',state['point'][1]));return 0
    for i,(offset,count,fn) in enumerate([(0x750,2,event),(0x6d8,0,lambda a:state['tick']),(0x6c0,2,pen)]):
        name='native_service_'+str(i);address=0x80001000+i*16;m.word(0x80800000+offset,address);m.write(address,bytes.fromhex('0800e00300000000'));m.symbols[name]=address;m.intercept(name,count,fn)
    def poll(code,tick,point):
        state.update(code=code,tick=tick,point=point);return m.call('poll',BUFFER)
    def gesture():return m.call('sango_touch_process',BUFFER)
    # Reproduce the observed stale origin: previous tap at240,84, new at200,75.
    assert poll(11,100,(240,84))==0
    assert poll(-1,101,(200,75))==0
    assert poll(-1,102,(200,75))==1;assert struct.unpack('<3I',m.read(BUFFER,12))==(1,0,200|(75<<16));assert gesture()==0
    assert poll(8,105,(200,75))==1;assert gesture()==1;assert m.read(BUFFER,4)==struct.pack('<I',11)
    # A contact shorter than the settle interval still yields exactly one tap.
    assert poll(11,200,(200,75))==0;assert poll(8,200,(460,20))==1;assert gesture()==0
    assert poll(-1,200,(460,20))==1;assert gesture()==1;assert m.read(BUFFER,4)==struct.pack('<I',11)
    # Settled contacts retain real movement and release; no click is invented.
    assert poll(11,300,(460,20))==0;assert poll(-1,302,(220,200))==1;assert gesture()==0
    assert poll(-1,305,(220,150))==1;assert gesture()==1;assert m.read(BUFFER,4)==struct.pack('<I',12)
    assert poll(8,308,(220,50))==1;assert gesture()==1;assert m.read(BUFFER,4)==struct.pack('<I',13)
    m.word(m.symbols['h1_screen_id'],9)
    assert poll(11,400,(220,50))==0;assert poll(-1,405,(200,75))==0
    assert poll(8,480,(200,75))==1;assert gesture()==0
    assert poll(-1,480,(200,75))==1;assert gesture()==1;assert m.read(BUFFER,4)==struct.pack('<I',11)
    return 4
def main():
    m=base.H1Machine(ELF);s=m.symbols;checks=check_native_contacts()
    for native,mode in [(1,0),(1,1),(0,2)]:
        m.word(s['h1_native_active'],native);m.word(s['h1_layout_game'],mode)
        for x,y in [(0,0),(479,215),(319,100),(400,100),(350,234),(430,252),(480,0),(0,272)]:
            ok=m.call('h1_map_touch',x,y,BUFFER,BUFFER+4)
            assert bool(ok)==(x<480 and y<272)
            if ok:
                mapped=struct.unpack('<II',m.read(BUFFER,8))
                assert mapped==(x,y)
            checks+=1
    pack=(ROOT/'dist/h1/sango.h1r').read_bytes();address=0x81000000;m.write(address,pack)
    assert m.call('h1_ui_use_buffer',address,len(pack),instruction_limit=500000000)==1;checks+=1
    m.write(address+len(pack)-1,bytes([pack[-1]^1]))
    assert m.call('h1_ui_use_buffer',address,len(pack),instruction_limit=500000000)==0;checks+=1
    m.write(address,pack);assert m.call('h1_ui_use_buffer',address,len(pack),instruction_limit=500000000)==1
    m.intercept('h1_ui_assets_load',0,lambda a:1);m.intercept('h1_native_flip',0,lambda a:0)
    m.call('h1_draw_main',2)
    raw=m.read(s['h1_native_pixels'],480*272*2)
    assert raw[:480*272]!=raw[480*272:]
    pixels=struct.unpack('<'+'H'*(480*272),raw)
    assert len(set(pixels))>500;checks+=1
    im=Image.new('RGB',(480,272));im.putdata([((p>>11)*255//31,((p>>5)&63)*255//63,(p&31)*255//31) for p in pixels]);im.save(ROOT/'reports/screenshots/mips-native-main.png')
    viewport=(ROOT/'tools/test_battle_viewport.py').read_text(encoding='utf8')
    viewport=viewport.replace("'build/sango.elf'","'build/h1/H1Sango.elf'").replace("'build/sango.lib'","'build/h1/sango.lib'").replace("'build/三国霸业.bda'","'dist/h1/H1Sango.bda'").replace("'build/battle-viewport-validation.json'","'reports/h1-landscape-battle.json'")
    viewport=viewport.replace('COLS, ROWS, TILE = 15, 13, 16','COLS, ROWS, TILE = 20, 9, 16')
    viewport=viewport.replace(
        "expected = b''.join(m.read(address+16+(top+row)*width+left, COLS)\n                                for row in range(ROWS))",
        "expected = bytes(m.read(address+16+(top+row)*width+left+col, 1)[0] if top+row<height and left+col<width else 0\n"
        "                             for row in range(10) for col in range(21))")
    viewport=viewport.replace('COLS*ROWS','210').replace('index % COLS','index % 21').replace('index//COLS','index//21')
    viewport=viewport.replace('<= 208','<= 144').replace('== 195','== 135')
    viewport=viewport.replace('args[1]+args[3] <= 144','args[1]+args[3] <= 160')
    viewport=viewport.replace('(0, 12, True), (14, 12, True)','(0, 8, True), (19, 8, True)').replace('(0, 13, False), (15, 0, False)','(0, 9, False), (20, 0, False)')
    viewport=viewport.replace('(0, 192, True), (224, 192, True), (0, 208, False)','(0, 128, True), (304, 128, True), (0, 144, False)').replace('(240, 0, False)','(320, 0, False)')
    viewport=viewport.replace('((12, True), (13, False), (15, False))','((8, True), (9, False), (11, False))').replace('if row == 12:','if row == 8:')
    viewport=viewport.replace('[15, 13]','[20, 9]').replace('[0, 0, 240, 208]','[0, 0, 320, 144]').replace('195-tile','210-tile with clipped margin')
    ns={'__name__':'h1_landscape_viewport','__file__':str(ROOT/'tools/test_battle_viewport.py')};exec(compile(viewport,ns['__file__'],'exec'),ns);ns['main']()
    mm,_,_,_,_=ns['machine'](110);marks=[]
    mm.word(STATE['g_move_costs_address'],BUFFER+512);mm.write(BUFFER+512,bytes([1])*225)
    mm.write(STATE['g_move_source_x'],bytes([16]));mm.intercept('sango_raster_draw_masked',6,lambda a:marks.append(a));mm.call('sango_battle_draw_move_range')
    assert len(marks)==36 and min(a[0] for a in marks)==256 and max(a[0] for a in marks)==304;checks+=1
    report={'result':'passed','layout_backend_checks':checks,'native_contact_cases':['stale-coordinate-tap','short-right-edge-tap','real-drag-and-release','menu-release-position-tap'],'battle_matrix':json.loads((ROOT/'reports/h1-landscape-battle.json').read_text('utf8')),'bda_sha256':hashlib.sha256((ROOT/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),'asset_pack_sha256':hashlib.sha256(pack).hexdigest(),'scope':'Actual H1 MIPS native-coordinate mapping and contact timing, valid/corrupt native art pack parsing, full landscape UI render, seven real battle maps, and movement markers in newly visible columns. Firmware services modeled.'}
    (ROOT/'reports/h1-landscape-mips.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report))
if __name__=='__main__':main()
