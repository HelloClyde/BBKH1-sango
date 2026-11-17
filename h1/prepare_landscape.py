"""Typeset and encode new landscape art; never alter the upstream asset set."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps
import hashlib,json,os,struct,zlib
ROOT=Path(__file__).resolve().parents[1];ASSET=ROOT/'assets/h1-landscape'
FONT=Path(os.environ.get('SANGO_FONT', ROOT/'assets/fonts/NotoSansCJKsc-Regular.otf'))
def font(n):return ImageFont.truetype(str(FONT),n)
def label(draw,box,value,size=20,color='#2c180d'):
    x,y,w,h=box;f=font(size);b=draw.textbbox((0,0),value,font=f)
    draw.text((x+(w-(b[2]-b[0]))/2,y+(h-(b[3]-b[1]))/2-b[1]),value,font=f,fill=color)
def main():
    out=ASSET/'native';out.mkdir(exist_ok=True)
    base=Image.open(ASSET/'generated/main-menu.png').convert('RGB').resize((480,272),Image.Resampling.LANCZOS)
    panel=Image.open(ASSET/'generated/panel.png').convert('RGB').resize((480,272),Image.Resampling.LANCZOS)
    main_rows=[(250,70,204,34),(250,111,204,34),(250,152,204,34),(250,193,204,34)]
    assets=[]
    for selected in range(4):
        im=base.copy();d=ImageDraw.Draw(im);label(d,(0,6,480,37),'三国霸业',24,'#f9deb0')
        for i,(box,title) in enumerate(zip(main_rows,['新君登陆','重返沙场','游戏设置','卸甲归田'])):
            if i==selected:
                x,y,w,h=box;d.rounded_rectangle((x-2,y-2,x+w+1,y+h+1),3,outline='#fff0ad',width=2)
            label(d,box,title,18)
        label(d,(8,251,464,17),'方向键选择 · 确定进入 · 返回取消',12,'#ead4a7')
        assets.append((f'main-{selected}',im))
    assets.append(('panel',panel))
    catalog=json.loads((ROOT/'assets/color/catalog.json').read_text('utf8'))
    f=next(f for f in catalog['frames'] if f['resource']==45)
    old=Image.open(ROOT/'assets/color'/f['color']).convert('RGB')
    im=panel.copy();d=ImageDraw.Draw(im);label(d,(0,4,480,34),'选择历史时期',20,'#f6ddb1')
    titles=['董卓弄权','曹操崛起','赤壁之战','三国鼎立'];years=['公元190年','公元198年','公元208年','公元225年']
    for i in range(4):
        x=12+i*116;d.rectangle((x,51,x+107,228),fill='#d8b57d',outline='#7d5126',width=2)
        art=old.crop((34,48+i*61,143,105+i*61));art=ImageOps.fit(art,(100,87),method=Image.Resampling.NEAREST)
        im.paste(art,(x+4,56));label(d,(x,150,108,32),titles[i],16);label(d,(x,192,108,25),years[i],12)
    label(d,(0,242,480,25),'左右选择 · 确定进入 · 返回主菜单',14,'#ead4a7');assets.append(('scenarios',im))
    hud=panel.crop((0,0,480,56));d=ImageDraw.Draw(hud);d.rectangle((4,3,475,53),fill='#302015',outline='#bc8b4d',width=1)
    assets.append(('hud',hud))
    data=bytearray(b'H1SGUI2\0'+struct.pack('<I',len(assets)))
    provenance=[]
    for name,im in assets:
        target=out/(name+'.png');im.save(target)
        rgb=bytearray()
        for r,g,b in im.getdata():rgb+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3))
        data+=struct.pack('<III',im.width,im.height,len(rgb))+rgb
        provenance.append({'name':name,'size':im.size,'png_sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
    data+=struct.pack('<I',zlib.crc32(data))
    (out/'sango.h1r').write_bytes(data)
    (ASSET/'manifest.json').write_text(json.dumps({'version':2,'target':[480,272],'generation':'Built-in image_gen; generated/main-menu.png and generated/panel.png; typed native UI text via Noto Sans CJK','assets':provenance,'pack_bytes':len(data),'pack_sha256':hashlib.sha256(data).hexdigest()},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('Native landscape assets:',len(assets),'pack bytes:',len(data))
if __name__=='__main__':main()
