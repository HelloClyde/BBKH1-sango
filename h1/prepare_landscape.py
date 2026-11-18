"""Typeset and encode new landscape art; never alter the upstream asset set."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageOps
import hashlib,json,struct,zlib
ROOT=Path(__file__).resolve().parents[1];ASSET=ROOT/'assets/h1-landscape'
from bitmap_font import text_mask,TOP_PAD

def label(draw,box,value,size=16,color='#2c180d'):
    x,y,w,h=box;mask=text_mask(value,size)
    draw.bitmap((int(x+(w-mask.width)//2),int(y+(h-size)//2)-TOP_PAD),mask,fill=color)

def grid_cells(name, columns, rows):
    """Crop generated illustration cells, inset a few source pixels at seams."""
    with Image.open(ASSET/'generated'/name) as original:
        sheet=original.convert('RGB')
    for row in range(rows):
        for col in range(columns):
            left=round(col*sheet.width/columns)
            top=round(row*sheet.height/rows)
            right=round((col+1)*sheet.width/columns)
            bottom=round((row+1)*sheet.height/rows)
            yield sheet.crop((left+3,top+3,right-3,bottom-3))

def main():
    out=ASSET/'native';out.mkdir(exist_ok=True)
    base=Image.open(ASSET/'generated/main-menu.png').convert('RGB').resize((480,272),Image.Resampling.LANCZOS)
    panel=Image.open(ASSET/'generated/panel.png').convert('RGB').resize((480,272),Image.Resampling.LANCZOS)
    title=Image.open(ASSET/'generated/title-gold.png').convert('RGBA')
    title_bounds=title.getchannel('A').point(lambda a:255 if a>8 else 0).getbbox()
    if title_bounds is None:
        raise ValueError('The title image has no visible lettering')
    title=ImageOps.contain(title.crop(title_bounds),(194,38),Image.Resampling.LANCZOS)
    base.paste(title,((480-title.width)//2, (48-title.height)//2),title)
    main_rows=[(250,70,204,34),(250,111,204,34),(250,152,204,34),(250,193,204,34)]
    assets=[]
    for selected in range(4):
        im=base.copy();d=ImageDraw.Draw(im)
        for i,(box,title) in enumerate(zip(main_rows,['新君登陆','重返沙场','游戏设置','卸甲归田'])):
            if i==selected:
                x,y,w,h=box;d.rounded_rectangle((x-2,y-2,x+w+1,y+h+1),3,outline='#fff0ad',width=2)
            label(d,box,title,16)
        label(d,(8,251,464,17),'方向键选择 · 确定进入 · 返回取消',12,'#ead4a7')
        assets.append((f'main-{selected}',im))
    assets.append(('panel',panel))
    im=panel.copy();d=ImageDraw.Draw(im);label(d,(0,4,480,34),'选择历史时期',24,'#f6ddb1')
    titles=['董卓弄权','曹操崛起','赤壁之战','三国鼎立'];years=['公元190年','公元198年','公元208年','公元225年']
    for i,scene in enumerate(grid_cells('periods-hd.png',2,2)):
        x=12+i*116;d.rectangle((x,51,x+107,228),fill='#d8b57d',outline='#7d5126',width=2)
        art=ImageOps.fit(scene,(100,87),method=Image.Resampling.LANCZOS)
        im.paste(art,(x+4,56));label(d,(x,150,108,32),titles[i],16);label(d,(x,192,108,25),years[i],12)
    label(d,(0,242,480,25),'左右选择 · 确定进入 · 返回主菜单',14,'#ead4a7');assets.append(('scenarios',im))
    hud=panel.crop((0,0,480,56));d=ImageDraw.Draw(hud);d.rectangle((4,3,475,53),fill='#302015',outline='#bc8b4d',width=1)
    assets.append(('hud',hud))
    # Slot order matches resource 53 and the existing 4x19 ruler mapping.
    portrait_size=72
    portrait_sources=(('rulers-hd-00.png',4,3),('rulers-hd-12.png',4,3),('rulers-hd-24.png',3,3))
    portraits=[cell for spec in portrait_sources for cell in grid_cells(*spec)]
    assert len(portraits)==33
    portrait_strip=Image.new('RGB',(portrait_size,portrait_size*len(portraits)))
    for i,portrait in enumerate(portraits):
        portrait_strip.paste(ImageOps.fit(portrait,(portrait_size,portrait_size),method=Image.Resampling.LANCZOS),
                             (0,i*portrait_size))
    assets.append(('ruler-portraits',portrait_strip))

    data=bytearray(b'H1SGUI3\0'+struct.pack('<I',len(assets)))
    provenance=[]
    for name,im in assets:
        target=out/(name+'.png');im.save(target)
        rgb=bytearray()
        for r,g,b in im.getdata():rgb+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3))
        data+=struct.pack('<III',im.width,im.height,len(rgb))+rgb
        provenance.append({'name':name,'size':im.size,'png_sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
    data+=struct.pack('<I',zlib.crc32(data))
    assert len(data)<=2000000
    (out/'sango.h1r').write_bytes(data)
    (ASSET/'manifest.json').write_text(json.dumps({'version':3,'target':[480,272],
        'generation':'Built-in image_gen artwork; WenQuanYi/Terminus interface text',
        'ruler_deck':{'resource_order':53,'count':33,'size':[72,72]},
        'generated_sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ASSET/'generated').glob('*.png'))},
        'assets':provenance,'pack_bytes':len(data),'pack_sha256':hashlib.sha256(data).hexdigest()},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('Native landscape assets:',len(assets),'pack bytes:',len(data))
if __name__=='__main__':main()
