"""Render the reported city page with the production MIPS UI and uniform fonts."""
import hashlib,json,struct
from PIL import Image
import test_mips as base
R=base.ROOT;S=base.module.STATE
def main():
    m=base.H1Machine(base.ELF);sym=m.symbols
    lib=(R/'dist/h1/sango.lib').read_bytes();art=(R/'dist/h1/sango.h1r').read_bytes()
    m.write(0x81000000,lib);m.word(S['g_resource_package_address'],0x81000000)
    m.write(0x81400000,art);assert m.call('h1_ui_use_buffer',0x81400000,len(art),instruction_limit=500000000)==1
    m.intercept('h1_ui_assets_load',0,lambda a:1)
    for name,address in [('g_character_table_address',0x81601000),('g_temp_character_ids_address',0x81605000),('g_army_slot_used_address',0x81606000),('g_army_rosters_address',0x81607000),('g_order_queue_address',0x81608000)]:m.word(S[name],address)
    m.call('sango_scenario_load',1,0x81605000)
    city=m.call('sango_character_find_city',13)
    # Model only native heap and user cancellation. Actual UI/font code runs.
    m.word(0x83c00010,0x80800000)
    for i,(offset,fn) in enumerate([(8,lambda a:0x81800000),(12,lambda a:0)]):
        name='footer_heap_'+str(i);pc=0x80001000+i*16;m.word(0x80800000+offset,pc);m.write(pc,bytes.fromhex('0800e00300000000'));sym[name]=pc;m.intercept(name,1,fn)
    captures=[]
    def present(args):
        if int.from_bytes(m.read(sym['h1_screen_id'],4),'little')==11:captures.append(m.read(sym['h1_native_pixels'],480*272*2))
    def cancel(args):m.write(args[0],struct.pack('<3I',5,1,0));return 0
    m.intercept('h1_native_flip',0,present);m.intercept('sango_input_next_event',1,cancel)
    assert m.call('sango_ui_show_city_details',city,instruction_limit=20000000)==0
    assert len(captures)==1
    values=struct.unpack('<'+'H'*(480*272),captures[0]);im=Image.new('RGB',(480,272));im.putdata([((p>>11)*255//31,((p>>5)&63)*255//63,(p&31)*255//31) for p in values]);im.save(R/'reports/screenshots/mips-uniform-city.png')
    footer='确定选择本城 · 返回取消'
    font_report=json.loads((R/'reports/font-validation.json').read_text('utf8'))
    assert font_report['uniform_cjk_font_pixels']['12']==12
    report={'result':'passed','bda_sha256':hashlib.sha256((R/'dist/h1/H1Sango.bda').read_bytes()).hexdigest(),'asset_pack_sha256':hashlib.sha256(art).hexdigest(),'screen':'city-details','footer':footer,'footer_cjk_nominal_pixels':12,'per_character_font_shrinking':False,'scope':'Actual MIPS UI and font rendering. Heap and cancel input modeled; not a firmware/physical-device test.'}
    (R/'reports/h1-uniform-footer.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(report,ensure_ascii=False))
if __name__=='__main__':main()
