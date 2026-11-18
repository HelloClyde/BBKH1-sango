"""Build the H1 port; keep the imported 9588 engine and assets reviewable."""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, zipfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/h1'
DIST=ROOT/'dist/h1'
SDK=Path(os.environ.get('H1_SDK', ROOT/'sdk')).resolve()
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(SDK))

def prepare():
    OUT.mkdir(parents=True,exist_ok=True); DIST.mkdir(parents=True,exist_ok=True)
    inc=OUT/'include'; inc.mkdir(exist_ok=True)
    # All old data views point into an owned, relocatable H1 buffer. Never
    # access the 9588 application's old address range on H1.
    def relocate(text):
        def sub(m):
            address=int(m[0].rstrip('uU'),16)
            return f'((uintptr_t)(sango_legacy_blob+0x{address-0x81c1fef0:x}))' if 0x81c1fef0<=address<0x81c2bb00 else m[0]
        return re.sub(r'0x81[cC][0-9a-fA-F]{5}[uU]?',sub,text)
    for p in (ROOT/'include').iterdir():
        if p.suffix in ('.h','.inc') and p.name!='bda_sango_compat.h':
            text=p.read_text(encoding='utf8')
            if p.name=='sango_types.h': text=text.replace('/* The reconstructed ABI','extern unsigned char sango_legacy_blob[];\n/* The reconstructed ABI')
            if p.name=='sango_battle.h':text=text.replace('SANGO_BATTLE_VIEW_COLUMNS = 15','SANGO_BATTLE_VIEW_COLUMNS = 20').replace('SANGO_BATTLE_VIEW_ROWS = 13','SANGO_BATTLE_VIEW_ROWS = 9')
            if p.name=='sango_statusbar.h':
                text=text.replace('SANGO_BATTLE_STATUS_TOP = SANGO_BATTLE_MAP_HEIGHT,','SANGO_BATTLE_STATUS_TOP = SANGO_BATTLE_MAP_HEIGHT + 16,')
                text=text.replace('SANGO_BATTLE_STATUS_HEIGHT = 24','SANGO_BATTLE_STATUS_HEIGHT = SANGO_STATUSBAR_TOP - SANGO_BATTLE_STATUS_TOP')
            (inc/p.name).write_text(relocate(text),encoding='utf8')
    old=(ROOT/'include/bda_sango_compat.h').read_text(encoding='utf8')
    declarations=re.findall(r'static inline ([^\{]+)\{',old)
    extra=(ROOT/'h1/api.h').read_text(encoding='utf8')
    (inc/'bda_sango_compat.h').write_text('#pragma once\n#include <stdint.h>\n#include <stddef.h>\n'+extra+'\n'+'\n'.join(x.strip()+';' for x in declarations),encoding='utf8')
    for name in ('bda_sdk.h','bda_filesystem.h'):
        (inc/name).write_text('#include "bda_sango_compat.h"\n',encoding='utf8')
    sources=[]
    for p in sorted((ROOT/'src').rglob('*.c')):
        if p.name in ('sango_entry.c','sango_music_sdk.c'): continue
        text=relocate(p.read_text(encoding='utf8'))
        if p.name=='sango_data.c': text=text.replace('section(".sango_data"), ','')+'\nunsigned int sango_original_data_size=sizeof(sango_original_data);\n'
        if p.name=='boot.c': text=text.replace('bda_memset(', 'h1_compat_memset(')
        if p.name=='start_menu.c':
            text=text.replace('    sango_music_set_scene(SANGO_MUSIC_MENU);','    h1_layout_game = 0;\n    sango_music_set_scene(SANGO_MUSIC_MENU);')
            text=text.replace('                    return 1;', '                    h1_layout_game = 1;\n                    return 1;')
            # Loaded sessions take the final return path.
            text=text.replace('    return 1;\n}', '    h1_layout_game = 1;\n    return 1;\n}',1)
        overrides={'sango_menu_select_animated','sango_menu_choose_ruler','sango_menu_show_credits','sango_intro_open_menu','sango_city_choose_command',
            'sango_animation_play_resource','sango_settings_show','sango_ui_choose_option','sango_ui_select_character',
            'sango_ui_select_item','sango_ui_select_number','sango_ui_show_message','sango_ui_show_character_message',
            'sango_ui_show_city_details','sango_save_draw_slots','sango_save_choose_slot','sango_strategy_draw_map',
            'sango_strategy_select_city','sango_strategy_present','sango_battle_copy_viewport_tiles','sango_battle_draw_terrain',
            'sango_battle_info_show','sango_battle_show_overview'}
        for name in overrides:
            text=re.sub(r'(^\w[^\n;]*?\b)'+name+r'(\s*\([^;]*?\)\s*\{)',r'\1'+name+r'_legacy\2',text,flags=re.M)
        if p.name=='controller.c':text=text.replace('    sango_music_set_scene(g_battle_control_mode', '    h1_layout_game=2; h1_native_active=0; h1_battle_camera_reset();\n    sango_music_set_scene(g_battle_control_mode')
        if p.parent.name=='battle':
            text=text.replace('g_battle_viewport_x + 0xf','g_battle_viewport_x + SANGO_BATTLE_VIEW_COLUMNS').replace('viewport_x + 0xf','viewport_x + SANGO_BATTLE_VIEW_COLUMNS')
            text=text.replace('* 0x10 & 0xff','* 0x10 & 0xffff')
            if p.name=='field.c':
                text=text.replace('g_battle_viewport_y) * 0xf +','g_battle_viewport_y) * 21 +')
                text=text.replace('>= SANGO_BATTLE_VIEW_COLUMNS ||','>= H1_BATTLE_DRAW_COLS ||').replace('>= SANGO_BATTLE_VIEW_ROWS)','>= H1_BATTLE_DRAW_ROWS)')
                text=text.replace('g_battle_viewport_x + SANGO_BATTLE_VIEW_COLUMNS','g_battle_viewport_x + H1_BATTLE_DRAW_COLS')
                text=text.replace('g_battle_viewport_y + SANGO_BATTLE_VIEW_ROWS','g_battle_viewport_y + H1_BATTLE_DRAW_ROWS')
                text=text.replace('>= SANGO_BATTLE_MAP_WIDTH || y < 0 || y >= SANGO_BATTLE_MAP_HEIGHT','>= H1_BATTLE_DRAW_COLS * 16 || y < 0 || y >= H1_BATTLE_DRAW_ROWS * 16')
            if p.name=='overview.c':
                text=text.replace('(viewport_column & 0xff) < 0xf','(viewport_column & 0xff) < H1_BATTLE_DRAW_COLS').replace('(viewport_column & 0xf)', '(viewport_column & 0xff)')
                text=text.replace('(viewport_row & 0xff) < SANGO_BATTLE_VIEW_ROWS','(viewport_row & 0xff) < H1_BATTLE_DRAW_ROWS')
            if p.name=='player_turn.c':text=text.replace('screen_y < SANGO_BATTLE_MAP_HEIGHT','screen_y < H1_BATTLE_DRAW_ROWS * 16')
        if p.name=='sango_color.c':text=text.replace('screen_x >= 240','screen_x >= H1_SURFACE_WIDTH').replace('x >= 240','x >= H1_SURFACE_WIDTH')
        if p.name=='sango_touch.c':text=text.replace('x >= 240 || y < 0 || y >= 320','x >= 480 || y < 0 || y >= 272')
        dest=OUT/(p.parent.name+'_'+p.name)
        dest.write_text(text,encoding='utf8'); sources.append(dest)
    return inc,sources

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', type=Path, help='Path to mipsel-none-elf-gcc[.exe]')
    parser.add_argument('--assets', action='store_true', help='Rebuild both resource libraries')
    args=parser.parse_args()
    from tools.sango_lib import pack,generate_includes
    OUT.mkdir(parents=True,exist_ok=True)
    if args.assets or not (OUT/'sango.lib').exists() or not (OUT/'sango.clr').exists():
        library=pack(ROOT/'assets/library');(OUT/'sango.lib').write_bytes(library)
        generate_includes(ROOT/'assets/library',ROOT/'include')
        from tools.color_assets import build
        color=build(OUT/'sango.clr')
    else:
        library=(OUT/'sango.lib').read_bytes()
        color={'sha256':hashlib.sha256((OUT/'sango.clr').read_bytes()).hexdigest()}
    inc,sources=prepare()
    suffix='.exe' if os.name=='nt' else ''
    cc=args.cc or (Path(os.environ.get('H1_GNU_BIN', ROOT/'.toolchain/bin'))/('mipsel-none-elf-gcc'+suffix))
    cc=cc.resolve()
    if not cc.is_file():
        raise SystemExit('Compiler missing. Run python tools/install_h1_toolchain.py or pass --cc / set H1_GNU_BIN.')
    os.environ['H1_GNU_BIN']=str(cc.parent)
    os.environ['SOURCE_DATE_EPOCH']='1794787200'
    from h1_bda.build import build_bda
    from h1_bda.validate import validate_bda
    elf=OUT/'H1Sango.elf'
    sources=[ROOT/'h1/entry.S',ROOT/'h1/backend.c',ROOT/'h1/native_ui.c',ROOT/'h1/music.c',ROOT/'h1/audio.c',*sources]
    data=build_bda(sources,'三国霸业',0x48,[inc,ROOT/'h1'],debug_elf=elf,
        icon_png=ROOT/'assets/app-icon/icon-0-32x32.png' if (ROOT/'assets/app-icon/icon-0-32x32.png').exists() else next((ROOT/'assets/app-icon').glob('icon-0-*.png')),
        defines=['SANGO_H1'],compiler_flags=['-std=gnu99','-fexec-charset=GBK','-fno-strict-aliasing','-fwrapv','-Wno-incompatible-pointer-types','-Wno-int-conversion','-Werror=implicit-function-declaration'])
    bda=DIST/'H1Sango.bda'; bda.write_bytes(data)
    validation=validate_bda(bda)
    assert validation['ok'],validation
    symbols={}
    for line in subprocess.check_output([str(cc.with_name('mipsel-none-elf-nm'+suffix)),'-S',str(elf)],text=True).splitlines():
        fields=line.split()
        if len(fields)==4:symbols[fields[3]]=[int(fields[0],16),int(fields[1],16)]
    assert symbols['h1_bda_main'][0]==0x83c00020
    assert symbols['h1_private_stack'][0]+symbols['h1_private_stack'][1]<0x83ff0000
    summary={'target':'H1/Y100 V1.41','resolution':[480,272],'upstream_commit':'8786793bd9c7c102b05b72cd9d88887d100ac60d',
        'sdk_commit':subprocess.check_output(['git','-C',str(SDK),'rev-parse','HEAD'],text=True).strip(),
        'compiler':subprocess.check_output([str(cc),'--version'],text=True).splitlines()[0],
        'bda_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'validation':validation,'symbols':symbols,
        'library_sha256':hashlib.sha256(library).hexdigest(),'color_sha256':color['sha256'],
        'inputs':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*sorted((ROOT/'h1').glob('*')), *sorted((ROOT/'src').rglob('*.c')), *sorted((ROOT/'include').glob('*.h'))] if p.is_file()}}
    (DIST/'BUILD_INFO.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    for name in ('sango.lib','sango.clr'):shutil.copy2(OUT/name,DIST/name)
    shutil.copy2(ROOT/'assets/h1-landscape/native/sango.h1r',DIST/'sango.h1r')
    package()
    print(json.dumps({k:summary[k] for k in ('target','bytes','bda_sha256')},ensure_ascii=False))

def package():
    metadata=json.loads((DIST/'BUILD_INFO.json').read_text(encoding='utf8'))
    metadata.update(version='0.2.3-h1-ui',native_asset_sha256=hashlib.sha256((DIST/'sango.h1r').read_bytes()).hexdigest(),
                    strategy_viewport=[12,6],battle_viewport=[20,9],body_font_pixels=14,title_font_pixels=24,
                    font_type='native WenQuanYi/Terminus BDF bitmap',title_integer_scale=2,
                    main_title='GPT Image gold lettering',native_asset_version=3,ruler_portrait_count=33,
                    strategy_vertical_scroll_pixels=120,battle_scroll_unit='lcd-pixel',
                    footer_font_pixels=12,uniform_cjk_nominal_size=True)
    (DIST/'BUILD_INFO.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    asset_root=ROOT/'assets/h1-landscape'
    with zipfile.ZipFile(DIST/'H1Sango-landscape-assets.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(asset_root.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(asset_root))
    files={'应用/程序/H1Sango.bda':DIST/'H1Sango.bda',
        '应用/数据/游戏/sango.lib':DIST/'sango.lib','应用/数据/游戏/sango.clr':DIST/'sango.clr',
        '应用/数据/游戏/sango.h1r':DIST/'sango.h1r'}
    for p in (ROOT/'assets/music/runtime').glob('*.wav'):files['应用/数据/游戏/SGMUSIC/'+p.name]=p
    for name in ('README.md','README-H1.md','LICENSE.md','THIRD_PARTY_NOTICES.md'):files[name]=ROOT/name
    for p in (ROOT/'LICENSES').iterdir():
        if p.is_file():files['LICENSES/'+p.name]=p
    files['BUILD_INFO.json']=DIST/'BUILD_INFO.json'
    report=ROOT/'reports/H1-VERIFICATION.md'
    if report.exists():files['H1-VERIFICATION.md']=report
    for name in ('h1-landscape-firmware.json','h1-landscape-mips.json','h1-landscape-battle.json','h1-landscape-battle-render.json','font-validation.json','h1-menu-wording.json','h1-uniform-footer.json','h1-uniform-font-firmware.json'):
        p=ROOT/'reports'/name
        if p.exists():files['reports/'+name]=p
    for name in ('mips-menu-wording.png','landscape-02-settings.png','landscape-05-liubei.png','uniform-font-city.png','landscape-11-save-slots.png','landscape-14-loaded-map.png','mips-native-battle.png'):
        p=ROOT/'reports/screenshots'/name
        if p.exists():files['reports/screenshots/'+name]=p
    hardware=ROOT/'reports/hardware-20261117'
    for name in ('physical-checks.json','deployment-summary.json','visual-review-city.json',
                 '10-main-confirmed.png','11-settings.png','12-scenario.png','13-ruler.png',
                 '14-map.png','15-city-uniform-footer.png','16-number-before.png','17-number-plus.png',
                 '18-save-slots.png','19-load-slots.png','20-loaded-map.png','21-desktop-after-exit.png'):
        if (hardware/name).exists():files['reports/hardware-20261117/'+name]=hardware/name
    for name in ('physical-checks.json','deployment-summary.json','save-path-load.png','save-path-saved.png'):
        p=ROOT/'reports/hardware-savepath-20261117'/name
        if p.exists():files['reports/hardware-savepath-20261117/'+name]=p
    p=ROOT/'reports/h1-save-paths.json'
    if p.exists():files['reports/h1-save-paths.json']=p
    for name in ('h1-popup-mips.json','H1-POPUP-CANDIDATE.md'):
        p=ROOT/'reports'/name
        if p.exists():files['reports/'+name]=p
    for name in ('mips-popup-system.png','mips-popup-cascade.png'):
        p=ROOT/'reports/screenshots'/name
        if p.exists():files['reports/screenshots/'+name]=p
    for name in ('h1-bitmap-font.json','H1-BITMAP-CANDIDATE.md','h1-pixel-scroll-mips.json','H1-PIXEL-SCROLL.md'):
        p=ROOT/'reports'/name
        if p.exists():files['reports/'+name]=p
    for name in ('mips-bitmap-main.png','mips-bitmap-settings.png','mips-bitmap-number.png','mips-uniform-city.png'):
        p=ROOT/'reports/screenshots'/name
        if p.exists():files['reports/screenshots/'+name]=p
    report=ROOT/'reports/h1-landscape-art-mips.json'
    if report.exists():files['reports/'+report.name]=report
    for name in ('mips-art-main.png','mips-art-periods.png','mips-art-ruler-liubei.png'):
        p=ROOT/'reports/screenshots'/name
        if p.exists():files['reports/screenshots/'+name]=p
    for directory,names in (
        ('hardware-bitmap-20261118',('physical-ui.json','domestic-popup.png','city.png')),
        ('hardware-scroll-20261118',('physical-scroll.json','released.png')),
    ):
        for name in names:
            p=ROOT/'reports'/directory/name
            if p.exists():files[f'reports/{directory}/{name}']=p
    with zipfile.ZipFile(DIST/'H1Sango-install.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name,p in files.items():
            if p.exists():z.write(p,name)
    (DIST/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in sorted(DIST.iterdir()) if p.suffix in ('.bda','.zip','.lib','.clr','.h1r')),encoding='ascii')

if __name__=='__main__':main()
