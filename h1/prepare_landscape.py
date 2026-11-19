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

def transparent_art(name):
    with Image.open(ASSET/'generated'/name) as source:
        sprite=source.convert('RGBA')
    alpha=sprite.getchannel('A')
    bounds=alpha.point(lambda a: 255 if a>=24 else 0).getbbox()
    if bounds is None or alpha.getextrema()!=(0,255):
        raise ValueError(f'{name} must include a transparent background')
    return sprite.crop(bounds)


def tinted_banner(banner, cloth):
    # Preserve the brass pole and alpha, using the silk's shaded red region
    # as the tint mask for the three ownership colors.
    pixels=[]
    for r,g,b,a in banner.getdata():
        tint=max(0,min(1,(r-2*g+b-12)/28))
        shade=(2*r+g+b)/520
        rgb=[round(base*(1-tint)+min(255,target*shade)*tint)
             for base,target in zip((r,g,b),cloth)]
        pixels.append((*rgb,a))
    image=Image.new('RGBA',banner.size)
    image.putdata(pixels)
    return ImageOps.contain(image,(7,14),Image.Resampling.LANCZOS)


def strategy_banner_frames():
    banner=ImageOps.contain(transparent_art('strategy-banner-hd.png'),(80,160),Image.Resampling.LANCZOS)
    colors=((183,65,43),(55,113,174))  # Other ruler, player.
    strip=Image.new('RGBA',(8,14*len(colors)))
    for i,cloth in enumerate(colors):
        strip.paste(tinted_banner(banner,cloth),(0,i*14))
    return strip


def credits_page():
    with Image.open(ASSET/'generated/credits-landscape.png') as source:
        page=source.convert('RGB').resize((480,272),Image.Resampling.LANCZOS)
    with Image.open(ROOT/'assets/color/credits/credits-240x320.png') as source:
        legacy=[source.crop(rect).convert('RGB') for rect in
                ((167,146,202,181),(174,182,210,216),(172,229,207,264))]
    with Image.open(ASSET/'generated/credits-helloclyde.png') as source:
        author=source.convert('RGB').resize((46,46),Image.Resampling.LANCZOS)
    portraits=[im.resize((46,46),Image.Resampling.NEAREST) for im in legacy]+[author]
    draw=ImageDraw.Draw(page)
    label(draw,(0,5,480,30),'制作群组',24,'#f6ddb1')
    label(draw,(0,57,480,25),'策划  BBK gamegroup',16)
    people=(('通宵虫','程序'),('南方小鬼','程序'),('Sunday','美工'),('HelloClyde','H1 移植'))
    for x,avatar,(name,role) in zip((77,186,294,403),portraits,people):
        draw.rounded_rectangle((x-26,108,x+25,159),radius=3,fill='#80603b',outline='#ddb27c')
        page.paste(avatar,(x-23,111))
        size=24 if name[0]>'\x7f' else 16
        label(draw,(x-50,163,100,22),name,size)
        label(draw,(x-50,186,100,15),role,12,'#634226')
    label(draw,(0,246,480,20),'点击或按键返回',12,'#ead4a7')
    return page


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
            label(d,box,title,24)
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

    with Image.open(ASSET/'generated/strategy-world-integrated.png') as source:
        world=source.convert('RGB').resize((480,360),Image.Resampling.LANCZOS)
    assets.append(('strategy-world',world))

    assets.append(('strategy-banner',strategy_banner_frames()))
    assets.append(('credits-landscape',credits_page()))

    data=bytearray(b'H1SGUI7\0'+struct.pack('<I',len(assets)))
    provenance=[]
    for name,im in assets:
        target=out/(name+'.png');im.save(target)
        rgb=bytearray()
        for r,g,b in im.convert('RGB').getdata():rgb+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3))
        if im.mode=='RGBA':rgb.extend(im.getchannel('A').tobytes())
        data+=struct.pack('<III',im.width,im.height,len(rgb))+rgb
        provenance.append({'name':name,'size':im.size,'png_sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
    data+=struct.pack('<I',zlib.crc32(data))
    assert len(data)<=2600000
    (out/'sango.h1r').write_bytes(data)
    (ASSET/'manifest.json').write_text(json.dumps({'version':7,'target':[480,272],
        'generation':'Built-in image_gen artwork; WenQuanYi/Terminus interface text',
        'ruler_deck':{'resource_order':53,'count':33,'size':[72,72]},
        'strategy_art':{'world':[480,360],'cities_in_world':38,'banner':[8,14],
                        'banner_frames':['other','player'],'banner_alpha':'8-bit plane after RGB565'},
        'generated_sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ASSET/'generated').glob('*.png'))},
        'assets':provenance,'pack_bytes':len(data),'pack_sha256':hashlib.sha256(data).hexdigest()},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('Native landscape assets:',len(assets),'pack bytes:',len(data))
if __name__=='__main__':main()
