"""Exercise the actual H1 MIPS ELF, including the new coordinate/raster backend.
The H1 firmware is modeled here; full firmware evidence is recorded separately.
"""
from pathlib import Path
import hashlib,json,struct,sys,types
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
source=(ROOT/'tools/test_control_flow.py').read_text(encoding='utf8').replace('self.u.mem_map(0, 0x02000000)','self.u.mem_map(0, 0x04000000)')
module=types.ModuleType('test_control_flow');module.__file__=str(ROOT/'tools/test_control_flow.py')
sys.modules[module.__name__]=module;exec(compile(source,module.__file__,'exec'),module.__dict__)
ELF=(ROOT/'build/h1/H1Sango.elf').read_bytes()
UI_ADDRESS=0x82000000  # Dedicated asset range, clear of the test stack and gameplay buffers.
original=module.Machine
symbols,_=module.decode_image(ELF)
module.STATE={key:symbols['sango_legacy_blob']+address-0x81c1fef0 for key,address in module.STATE.items()}
class H1Machine(original):
    def __init__(self,image):
        super().__init__(image)
        n=int.from_bytes(self.read(self.symbols['sango_original_data_size'],4),'little')
        data=bytearray(self.read(self.symbols['sango_original_data'],n))
        for i in range(0,n-3,4):
            value=int.from_bytes(data[i:i+4],'little')
            if 0x81c1fef0<=value<0x81c2bb00:data[i:i+4]=struct.pack('<I',self.symbols['sango_legacy_blob']+value-0x81c1fef0)
        self.write(self.symbols['sango_legacy_blob'],data)
        self.intercept('sango_music_poll',0,lambda a:0)
module.Machine=H1Machine

def main():
    checks=0
    m=H1Machine(ELF)
    # Every boundary rectangle must map to its displayed source, while the
    # control panel/gaps must never accidentally select a map tile.
    for mode in (0,1):
        m.word(symbols['h1_layout_game'],mode)
        points=([(0,0),(271,262),(280,0),(479,73),(280,74),(479,271),(272,0)] if mode else [(32,0),(235,271),(31,0),(236,271),(300,120)])
        for x,y in points:
            result=m.call('h1_map_touch',x,y,module.BUFFER,module.BUFFER+4)
            expected=(x<272 and y<263) or (280<=x<480 and y<74) if mode else 32<=x<236
            assert bool(result)==bool(expected)
            if expected:
                sx,sy=struct.unpack('<II',m.read(module.BUFFER,8))
                target=(x*240//272,y*232//263) if mode and x<272 else (((x-280)*240//200,232+y*88//74) if mode else ((x-32)*240//204,y*320//272))
                assert (sx,sy)==target
            checks+=1
    window=symbols['window']
    m.call('bda_sango_gui_set_fill_object',window,0x1234)
    m.call('bda_sango_gui_fill_region',window,-5,-5,15,15)
    assert m.read(window,2)==b'\x34\x12'
    assert m.read(window+2*(10*240+10),2)==b'\0\0';checks+=1
    # Actual software context copy must respect color key and copy black.
    other=0x81200000;m.write(other,bytes(153624));m.write(other,struct.pack('<3H',0,0xffff,0x5678))
    m.call('bda_sango_gui_context_copy',other,0,0,3,1,window,0,0,0xffff)
    assert m.read(window,6)==struct.pack('<3H',0,0x1234,0x5678);checks+=1
    m.call('bda_sango_gui_set_text_color',window,0xffff);m.call('bda_sango_gui_set_text_mode',window,1)
    m.write(module.BUFFER,'刘备'.encode('gbk')+b'\0')
    m.call('bda_sango_gui_draw_text',window,20,20,module.BUFFER,0xffffffff)
    pixels=m.read(window+2*20*240,2*12*240)
    assert pixels.count(b'\xff\xff')>20;checks+=1
    # Reuse the upstream real-map matrix against this H1 ELF and relocated
    # state. Model only its documented drawing/music service boundaries.
    viewport=(ROOT/'tools/test_battle_viewport.py').read_text(encoding='utf8')
    viewport=viewport.replace("'build/sango.elf'","'build/h1/H1Sango.elf'").replace("'build/sango.lib'","'build/h1/sango.lib'").replace("'build/三国霸业.bda'","'dist/h1/H1Sango.bda'").replace("'build/battle-viewport-validation.json'","'reports/h1-battle-viewport.json'")
    ns={'__name__':'h1_viewport','__file__':str(ROOT/'tools/test_battle_viewport.py')};exec(compile(viewport,ns['__file__'],'exec'),ns);ns['main']()
    report={'result':'passed','h1_backend_checks':checks,'elf_sha256':hashlib.sha256(ELF).hexdigest(),'bda_sha256':hashlib.sha256((ROOT/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),'scope':'Compiled H1 MIPS coordinate mapping, clipped raster, opaque/keyed context copies, GBK text, and upstream seven-map battle viewport/scroll/overlay matrix. Firmware services modeled; not a complete battle or physical-device test.'}
    (ROOT/'reports/h1-mips.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report))
if __name__=='__main__':main()
