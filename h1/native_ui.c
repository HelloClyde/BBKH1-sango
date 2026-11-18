/* Native 480x272 presentation. Gameplay state and save records remain upstream. */
#include "sango_internal.h"
#include "h1_sdk.h"
#include "string.h"
#include "sango_ruler_portraits.inc"
uint16_t h1_native_pixels[480*272];
int h1_native_active=1;
unsigned int h1_screen_id;
static unsigned char *art;
static uint16_t *strategy_terrain;
static int strategy_y;
static const uint16_t *images[7];
static unsigned heights[7];
enum {RULER_PORTRAIT_SIZE=72, RULER_PORTRAIT_COUNT=33};
static const uint16_t *ruler_portraits;
static unsigned read32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int h1_ui_use_buffer(void *p,unsigned n){
    const unsigned char *b=p;
    if(!b||n<16||n>2000000||read32(b)!=0x47533148)return 0;
    unsigned count=read32(b+8);
    unsigned version=read32(b+4);
    if(!((version==0x00324955&&count==7)||(version==0x00334955&&count==8)))return 0;
    unsigned crc=0xffffffff;for(unsigned i=0;i<n-4;i++){crc^=b[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}
    if((crc^0xffffffff)!=read32(b+n-4))return 0;
    const uint16_t *pending[7];unsigned ph[7];
    unsigned off=12;
    const uint16_t *portraits=0;
    for(unsigned i=0;i<count;i++){
        if(off+12>n-4)return 0;
        unsigned width=read32(b+off),height=read32(b+off+4),len=read32(b+off+8);
        unsigned expected_width=i==7?RULER_PORTRAIT_SIZE:480;
        unsigned expected_height=i==7?RULER_PORTRAIT_SIZE*RULER_PORTRAIT_COUNT:i==6?56:272;
        off+=12;
        if(width!=expected_width||height!=expected_height||len!=width*height*2||len>n-4-off)return 0;
        if(i==7)portraits=(const uint16_t *)(b+off);
        else{pending[i]=(const uint16_t *)(b+off);ph[i]=height;}
        off+=len;
    }
    if(off!=n-4)return 0;
    for(int i=0;i<7;i++){images[i]=pending[i];heights[i]=ph[i];}
    ruler_portraits=portraits;
    return 1;
}
int h1_ui_assets_load(void){
    if(art)return 1;void *f=bda_sango_fs_fopen("sango.h1r","rb");if(!f)return 0;
    h1_fseek(f,0,2);int n=h1_ftell(f);h1_fseek(f,0,0);
    if(n<=0||n>2000000){h1_fclose(f);return 0;}void *p=h1_alloc(n);
    int ok=p&&h1_fread(p,1,n,f)==(unsigned)n&&h1_ui_use_buffer(p,n);h1_fclose(f);
    if(!ok){if(p)h1_free(p);return 0;}art=p;return 1;
}
void h1_ui_assets_free(void){if(art)h1_free(art);art=0;ruler_portraits=0;if(strategy_terrain)h1_free(strategy_terrain);strategy_terrain=0;for(int i=0;i<7;i++)images[i]=0;}
void h1_ui_asset(unsigned id,int x,int y){if(id>=7||!images[id])return;for(unsigned j=0;j<heights[id];j++)for(int i=0;i<480;i++)if((unsigned)(x+i)<480&&(unsigned)(y+j)<272)h1_native_pixels[(y+j)*480+x+i]=images[id][j*480+i];}
static void box(int x,int y,int w,int h,unsigned c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)if((unsigned)i<480&&(unsigned)j<272)h1_native_pixels[j*480+i]=c;}
static void frame(int x,int y,int w,int h,unsigned c){box(x,y,w,1,c);box(x,y+h-1,w,1,c);box(x,y,1,h,c);box(x+w-1,y,1,h,c);}
static void txt(int x,int y,const char *p){h1_native_text(x,y,p,14,0x2983);}
static int length(const char *s){int n=0;while(*s++){n++;}return n;}
static void center(int x,int y,int w,const char *p,int size,unsigned c){h1_native_text(x+(w-length(p)*size/2)/2,y,p,size,c);}
static void number(int x,int y,unsigned value,unsigned color){char s[16];sango_format_integer(value,(uint8_t *)s,10);h1_native_text(x,y,s,14,color);}
static void page(const char *title){
    h1_native_active=1;h1_screen_id=20;if(!h1_ui_assets_load()){box(0,0,480,272,0xded1);}else h1_ui_asset(4,0,0);
    center(0,8,480,title,24,0xff16);
}
static void button(int x,int y,int w,int h,const char *text,int selected){box(x,y,w,h,selected?0x7b84:0xd5ec);frame(x,y,w,h,0x8a65);if(selected)frame(x+2,y+2,w-4,h-4,0xff16);center(x,y+(h-14)/2,w,text,14,selected?0xffbc:0x2983);}
static int hit(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
static int next(int *x,int *y){int e[3];for(;;){sango_input_next_event(e);if(e[0]==5)return e[1];if(e[0]==SANGO_TOUCH_TAP){*x=sango_touch_x(e);*y=sango_touch_y(e);return -1;}if(e[0]==SANGO_TOUCH_PAN_END){int k=sango_touch_swipe_key(e);if(k)return k;}}}
static void foot(const char *p){center(0,247,480,p,12,0xef57);}
typedef struct {uint16_t *pixels;int active;unsigned screen;} Saved;
static Saved save_screen(void){Saved s={h1_alloc(sizeof(h1_native_pixels)),h1_native_active,h1_screen_id};if(s.pixels){if(!s.active&&h1_layout_game==2)h1_compose_battle();memcpy(s.pixels,h1_native_pixels,sizeof(h1_native_pixels));}return s;}
static void restore(Saved s){if(s.pixels){memcpy(h1_native_pixels,s.pixels,sizeof(h1_native_pixels));h1_free(s.pixels);}h1_native_active=s.active;h1_screen_id=s.screen;h1_native_flip();}
static void blit_sprite(uint16_t *canvas,int height,const uint8_t *p,int sw,int sh,int mask,int x,int y,int w,int h){
    const uint16_t *rgb=sango_color_pixels((unsigned)p,sw,sh,mask);unsigned flags=sango_color_frame_flags((unsigned)p,sw,sh,mask),stride=(sw+3)/4;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
        int sx=i*sw/w,sy=j*sh/h;unsigned gray=(p[sy*stride+sx/4]>>(6-2*(sx&3)))&3;
        if(mask&&!(flags&SANGO_COLOR_TRANSPARENT_ZERO)&&(gray&1))continue;
        unsigned c=rgb?rgb[sy*sw+sx]:(3-gray)*0x52aa;if((flags&SANGO_COLOR_TRANSPARENT_ZERO)&&!c)continue;
        if((unsigned)(x+i)<480&&(unsigned)(y+j)<(unsigned)height)canvas[(y+j)*480+x+i]=c;
    }
}
static void sprite_pixels(const uint8_t *p,int sw,int sh,int mask,int x,int y,int w,int h){
    blit_sprite(h1_native_pixels,272,p,sw,sh,mask,x,y,w,h);
}
static void sprite(unsigned resource,unsigned index,int x,int y,int w,int h){
    const uint8_t *b=(const uint8_t *)sango_resource_item(resource,1,g_resource_package_address);if(!b||index>=b[4])return;
    unsigned sw=b[0]|b[1]<<8,sh=b[2]|b[3]<<8,stride=((sw+3)/4)*sh*((b[5]&1)?2:1);
    sprite_pixels(b+6+index*stride,sw,sh,b[5]&1,x,y,w,h);
}
static int ruler_portrait(unsigned scenario,unsigned who,int x,int y,int size){
    if(!ruler_portraits||scenario<1||scenario>4||who>=19)return 0;
    unsigned frame=ruler_portrait_frames[scenario-1][who];
    if(frame>=RULER_PORTRAIT_COUNT)return 0;
    const uint16_t *source=ruler_portraits+frame*RULER_PORTRAIT_SIZE*RULER_PORTRAIT_SIZE;
    for(int row=0;row<size;row++){
        int source_row=row*RULER_PORTRAIT_SIZE/size;
        uint16_t *target=h1_native_pixels+(y+row)*480+x;
        if(size==RULER_PORTRAIT_SIZE)
            memcpy(target,source+source_row*RULER_PORTRAIT_SIZE,RULER_PORTRAIT_SIZE*2);
        else
            for(int col=0;col<size;col++)
                target[col]=source[source_row*RULER_PORTRAIT_SIZE+col*RULER_PORTRAIT_SIZE/size];
    }
    return 1;
}
void h1_draw_main(unsigned selected){h1_native_active=1;h1_screen_id=1;h1_ui_assets_load();h1_ui_asset(selected&3,0,0);h1_native_flip();}
void sango_intro_open_menu(void){h1_draw_main(0);}
uint32_t sango_animation_play_resource_legacy(uint16_t);
uint32_t sango_animation_play_resource(uint16_t resource){if(resource==3){h1_draw_main(0);return 0xff;}return sango_animation_play_resource_legacy(resource);}
uint32_t sango_menu_select_animated(uint16_t background,short base){
    (void)base;int selected=0,x=0,y=0;h1_layout_game=0;strategy_y=0;
    for(;;){
        if(background==44)h1_draw_main(selected);
        else{h1_native_active=1;h1_screen_id=2;h1_ui_asset(5,0,0);frame(10+selected*116,49,112,182,0x821);frame(11+selected*116,50,110,180,0xff16);h1_native_flip();}
        int key=next(&x,&y);if(key==1)return 255;if(key==0x1c)return selected;
        if(key==-1){if(background==44){for(int i=0;i<4;i++)if(hit(x,y,247,68+41*i,212,38))return i;}else if(y>=50&&y<232&&x>=12&&x<476)return (x-12)/116;}
        else if(key==0x67||key==0x69)selected=(selected+3)&3;else if(key==0x6c||key==0x6a)selected=(selected+1)&3;
    }
}
uint32_t sango_menu_choose_ruler(uint32_t count){
    if(!count)return 255;unsigned selected=0,first=0;int x=0,y=0;char name[32];
    for(;;){
        page("选择君主");h1_screen_id=3;box(12,48,142,181,0xded1);frame(12,48,142,181,0x9a85);txt(27,52,"天下群雄");
        for(unsigned row=0;row<6&&first+row<count;row++){unsigned i=first+row;int yy=78+row*24;sango_load_named_text(g_temp_character_ids_address[i],name);button(20,yy,126,22,name,i==selected);}
        txt(176,52,"势力分布");sprite(47,0,168,78,168,128);
        unsigned who=g_temp_character_ids_address[selected],scenario=g_scenario_id_view2;
        if(!ruler_portrait(scenario,who,370,66,72)){
            if(scenario>=1&&scenario<=4&&who<19&&ruler_portrait_frames[scenario-1][who]!=255)
                sprite(53,ruler_portrait_frames[scenario-1][who],370,66,72,72);
            else sprite(scenario+47,who,370,66,72,72);
        }
        sango_load_named_text(who,name);center(342,148,124,name,24,0x2983);
        uint8_t cities[40];unsigned cities_count=sango_faction_list_cities(who,(int)cities);int positions=sango_resource_item(2,6,g_resource_package_address);
        for(unsigned i=0;i<cities_count;i++){uint8_t *p=(uint8_t *)positions+2*cities[i];frame(168+(p[0]*7+2)*2,78+(p[1]*7+2)*2,8,8,0xf800);}
        txt(354,181,"城池");number(410,181,cities_count,0x2983);
        button(248,212,100,27,"返回",0);button(360,212,100,27,"确定",1);foot("上下选择 · 左右翻页 · 点击名字预览");h1_native_flip();
        int key=next(&x,&y);if(key==1||(key==-1&&hit(x,y,248,212,100,27)))return 255;
        if(key==0x1c||(key==-1&&hit(x,y,360,212,100,27)))return who;
        if(key==-1&&hit(x,y,20,78,126,144)){unsigned i=first+(y-78)/24;if(i<count)selected=i;}
        if(key==0x67&&selected)selected--;if(key==0x6c&&selected+1<count)selected++;
        if(key==0x69)selected=selected>=6?selected-6:0;if(key==0x6a)selected=selected+6<count?selected+6:count-1;
        if(selected<first)first=selected;if(selected>=first+6)first=selected-5;
    }
}
void sango_menu_show_credits(void){Saved s=save_screen();page("制作群组");h1_screen_id=5;txt(34,58,"策划  BBK gamegroup");txt(34,88,"程序  通宵虫、南方小鬼");txt(34,118,"美工  Sunday");txt(34,158,"重建与优化  HelloClyde");txt(34,188,"H1 横屏适配 · 480 × 272");foot("点击或按键返回");h1_native_flip();int x,y;next(&x,&y);restore(s);}
void sango_settings_show(void){
    Saved saved=save_screen();int selected=0,x=0,y=0;
    for(;;){page("游戏设置");h1_screen_id=4;txt(42,65,"背景音乐");button(260,56,170,34,sango_music_is_enabled()?"开启":"关闭",selected==0);
        txt(42,118,"音乐音量");button(200,106,48,34,"－",selected==1);button(382,106,48,34,"＋",selected==1);number(291,115,sango_music_get_volume()*10,0x2983);txt(327,115,"%");
        button(38,164,190,42,"制作群组",selected==2);button(252,164,190,42,"保存并返回",selected==3);foot("方向键调整 · 点击按钮操作");h1_native_flip();
        int key=next(&x,&y),action=-1;if(key==1)action=3;
        if(key==0x67)selected=(selected+3)&3;if(key==0x6c)selected=(selected+1)&3;
        if(key==0x1c)action=selected;if(key==0x69&&selected==1&&sango_music_get_volume())sango_music_set_volume(sango_music_get_volume()-1);
        if(key==0x6a&&selected==1&&sango_music_get_volume()<10)sango_music_set_volume(sango_music_get_volume()+1);
        if(key==-1){if(hit(x,y,260,56,170,34))action=0;if(hit(x,y,200,106,48,34)&&sango_music_get_volume())sango_music_set_volume(sango_music_get_volume()-1);if(hit(x,y,382,106,48,34)&&sango_music_get_volume()<10)sango_music_set_volume(sango_music_get_volume()+1);if(hit(x,y,38,164,190,42))action=2;if(hit(x,y,252,164,190,42))action=3;}
        if(action==0)sango_music_set_enabled(!sango_music_is_enabled());if(action==1)sango_music_set_volume((sango_music_get_volume()+1)%11);
        if(action==2)sango_menu_show_credits();
        if(action==3){uint8_t b[8]={'S','G','S',1,0,0,0,0};b[4]=sango_music_is_enabled();b[5]=sango_music_get_volume();b[7]=0xa5^b[4]^b[5];sango_settings_sdk_write(b,8);restore(saved);return;}
    }
}
static Saved *command_background;
static unsigned command_left;
static unsigned menu(char labels[][80],unsigned count,unsigned selected,const Saved *saved,int ax,int ay){
    int x=0,y=0,w=76;unsigned first=0,visible=count<7?count:7;if(!count||!saved->pixels)return 255;
    for(unsigned i=0;i<count;i++){int n=length(labels[i]);while(n&&labels[i][n-1]==' ')labels[i][--n]=0;if(n*7+24>w)w=n*7+24;}
    if(count>visible)w+=8;if(w>228)w=228;
    int h=visible*26+12;if(ax<8)ax=8;if(ax>472-w)ax=472-w;if(ay<7)ay=7;if(ay>232-h)ay=232-h;
    for(;;){if(selected<first)first=selected;if(selected>=first+visible)first=selected-visible+1;
        memcpy(h1_native_pixels,saved->pixels,sizeof(h1_native_pixels));h1_native_active=1;h1_screen_id=20;
        box(ax+3,ay+3,w,h,0x3020);box(ax,ay,w,h,0xeed3);frame(ax,ay,w,h,0x820);frame(ax+2,ay+2,w-4,h-4,0xc58b);
        for(unsigned row=0;row<visible;row++){
            unsigned i=first+row;int yy=ay+6+row*26;if(i==selected)box(ax+5,yy,w-10-(count>visible?8:0),26,0xc58b);
            char clipped[80];unsigned n=0;int pixels=0,limit=w-24-(count>visible?8:0);const unsigned char *p=(unsigned char *)labels[i];
            while(*p){unsigned bytes=*p>=128&&p[1]?2:1;int advance=bytes==2?14:7;if(pixels+advance>limit)break;pixels+=advance;clipped[n++]=*p++;if(bytes==2)clipped[n++]=*p++;}clipped[n]=0;txt(ax+12,yy+6,clipped);
        }
        if(count>visible){int track=visible*26-4,thumb=track*visible/count;if(thumb<8)thumb=8;box(ax+w-9,ay+8,3,track,0xd5ec);box(ax+w-9,ay+8+(track-thumb)*first/(count-visible),3,thumb,0x8a65);}
        h1_native_flip();int key=next(&x,&y);
        if(key==1)return 255;if(key==0x1c)return selected;
        if(key==0x67)selected=(selected+count-1)%count;if(key==0x6c)selected=(selected+1)%count;
        if(key==-1){if(!hit(x,y,ax,ay,w,h))return 255;if(hit(x,y,ax+5,ay+6,w-10-(count>visible?8:0),visible*26))return first+(y-ay-6)/26;}
    }
}
uint32_t sango_ui_choose_option(uint16_t *b,uint32_t initial,char *text){
    if(command_background){if(b[0]<command_left)command_left=b[0];if(b[0]==command_left)memcpy(h1_native_pixels,command_background->pixels,sizeof(h1_native_pixels));}
    Saved saved=save_screen();if(!saved.pixels)return 255;char rows[40][80];unsigned stride=(b[2]-b[0])/6;if(!stride)stride=1;unsigned count=length(text)/stride;if(count>40)count=40;
    for(unsigned i=0;i<count;i++){unsigned n=stride<79?stride:79;memcpy(rows[i],text+i*stride,n);rows[i][n]=0;}
    unsigned choice=menu(rows,count,initial<count?initial:0,&saved,b[0]*2-6,(int)b[1]-3);
    if(command_background&&choice!=255)h1_free(saved.pixels);else restore(saved);return choice;
}
uint32_t sango_city_choose_command_legacy(void);
uint32_t sango_city_choose_command(void){
    Saved saved=save_screen();if(!saved.pixels)return 255;Saved *previous=command_background;unsigned previous_left=command_left;
    command_background=&saved;command_left=65535;unsigned choice=sango_city_choose_command_legacy();
    command_background=previous;command_left=previous_left;restore(saved);return choice;
}
uint32_t sango_ui_select_character(int ids,uint32_t count,uint32_t l,uint32_t t,uint8_t r,uint8_t b){
    (void)l;(void)t;(void)r;(void)b;if(!count)return 255;Saved saved=save_screen();unsigned selected=0,first=0;int x,y;char name[32];
    for(;;){if(selected<first)first=selected;if(selected>=first+7)first=selected-6;page("选择武将");h1_screen_id=7;
        txt(28,48,"姓名");txt(124,48,"归属");txt(220,48,"等级");txt(278,48,"武力");txt(336,48,"智力");txt(398,48,"兵力");
        for(unsigned row=0;row<7&&first+row<count;row++){
            unsigned i=first+row,who=((uint8_t *)ids)[i];int yy=73+row*23;SangoCharacter *c=(SangoCharacter *)(g_character_table_address+16*who);
            if(i==selected)box(20,yy-2,440,23,0xc58b);sango_load_named_text(who,name);txt(28,yy,name);sango_character_format_attribute(who,0,(uint8_t *)name);txt(124,yy,name);
            number(228,yy,c->level,0x2983);number(286,yy,c->strength,0x2983);number(344,yy,c->intelligence,0x2983);number(398,yy,c->troops,0x2983);
        }foot("上下选择 · 确定选将 · 返回结束选择");h1_native_flip();int key=next(&x,&y);
        if(key==1){restore(saved);return 255;}if(key==0x1c){restore(saved);return selected;}
        if(key==0x67)selected=(selected+count-1)%count;if(key==0x6c)selected=(selected+1)%count;
        if(key==-1&&hit(x,y,20,71,440,161)){unsigned i=first+(y-71)/23;if(i<count){restore(saved);return i;}}
    }
}
uint32_t sango_ui_select_item(int ids,uint32_t count,uint32_t l,uint32_t t,uint8_t r,uint8_t b){(void)r;(void)b;Saved saved=save_screen();if(!saved.pixels)return 255;char rows[40][80];if(count>40)count=40;for(unsigned i=0;i<count;i++)sango_item_read_name(((uint8_t *)ids)[i],rows[i]);unsigned result=menu(rows,count,0,&saved,l*2-6,(int)t-3);restore(saved);return result;}
uint32_t sango_ui_select_number(uint32_t initial,uint32_t maximum){
    Saved saved=save_screen();unsigned value=initial>maximum?maximum:initial,step=1;if(maximum>9999)maximum=9999;if(value>maximum)value=maximum;int x,y;
    for(;;){page("输入数量");h1_screen_id=8;txt(44,68,"上限");number(108,68,maximum,0x2983);txt(44,102,"每次调整");number(140,102,step,0x2983);
        button(60,148,72,42,"－",0);box(150,135,180,62,0xeed3);frame(150,135,180,62,0x9285);char s[16];sango_format_integer(value,(uint8_t *)s,10);center(150,151,180,s,24,0x2983);button(348,148,72,42,"＋",0);
        button(254,211,98,28,"返回",0);button(364,211,98,28,"确定",1);foot("左右选步长 · 上下改数量 · 点击加减");h1_native_flip();int k=next(&x,&y);
        if(k==1||(k==-1&&hit(x,y,254,211,98,28))){restore(saved);return 65535;}if(k==0x1c||(k==-1&&hit(x,y,364,211,98,28))){restore(saved);return value;}
        if(k==0x69&&step<1000)step*=10;if(k==0x6a&&step>1)step/=10;
        if(k==0x67||(k==-1&&hit(x,y,348,148,72,42)))value=value+step>maximum?maximum:value+step;
        if(k==0x6c||(k==-1&&hit(x,y,60,148,72,42)))value=value<step?0:value-step;
    }
}
static void paragraph(int x,int y,int width,const char *p){char line[80];while(*p&&y<228){unsigned n=0;while(*p&&n<(unsigned)(width/8)&&n<76){unsigned bytes=(uint8_t)*p>=128?2:1;if(n+bytes>(unsigned)(width/8))break;line[n++]=*p++;if(bytes==2&&*p)line[n++]=*p++;}line[n]=0;txt(x,y,line);y+=22;}}
void sango_ui_show_message(char *p,uint32_t wait){Saved saved=save_screen();page("消息");h1_screen_id=12;paragraph(36,64,408,p);foot("点击或按键继续");h1_native_flip();if(wait){sango_input_wait((wait&255)*40,1);restore(saved);}else if(saved.pixels)h1_free(saved.pixels);}
void sango_ui_show_character_message(uint8_t who,char *p){Saved saved=save_screen();char name[32];sango_load_named_text(who,name);page(name);if(!ruler_portrait(g_scenario_id_view2,who,28,67,72))sprite(g_scenario_id_view2+47,who,28,67,72,72);paragraph(124,64,320,p);foot("点击或按键继续");h1_native_flip();sango_input_wait(100,1);restore(saved);}
uint32_t sango_ui_show_city_details(uint8_t city){Saved saved=save_screen();char name[80],value[80];sango_city_read_name(city,name);page(name);h1_screen_id=11;for(unsigned i=0;i<11;i++){sango_city_format_attribute(city,i,value,315);txt(26+(i%2)*232,58+(i/2)*27,value);}foot("确定选择本城 · 返回取消");h1_native_flip();int x,y,k=next(&x,&y);restore(saved);return k==0x1c||k==-1;}
void sango_save_draw_slots(void){}
uint32_t sango_save_choose_slot(char load){
    Saved saved=save_screen();unsigned selected=0;int x,y;char name[32];
    for(;;){page(load?"读取进度":"保存进度");h1_screen_id=load?10:9;
        for(int i=0;i<3;i++){int yy=54+i*55;box(20,yy,440,46,i==(int)selected?0xc58b:0xeed3);frame(20,yy,440,46,i==(int)selected?0x820:0xa306);txt(34,yy+14,"档位");number(78,yy+14,i+1,0x2983);
            char path[]="sango0.sav";path[5]='0'+i*2;void *f=bda_sango_fs_fopen(path,"rb");if(f){uint8_t h[8];unsigned n=h1_fread(h,1,8,f);h1_fclose(f);if(n==8){unsigned old=g_scenario_id_view2;g_scenario_id_view2=h[0];sango_load_named_text(h[1],name);g_scenario_id_view2=old;txt(128,yy+14,name);number(262,yy+14,h[2]|h[3]<<8,0x2983);txt(298,yy+14,"年");number(334,yy+14,h[7],0x2983);txt(362,yy+14,"月");}}else txt(154,yy+14,"空白进度");
        }button(244,222,100,20,"返回",0);button(356,222,104,20,load?"读取":"保存",1);foot("三个独立槽位 · 原双文件存档格式");h1_native_flip();int k=next(&x,&y);
        if(k==1||(k==-1&&hit(x,y,244,222,100,20))){restore(saved);return 255;}
        int commit=k==0x1c||(k==-1&&hit(x,y,356,222,104,20));
        if(k==-1&&hit(x,y,20,54,440,165)){selected=(y-54)/55;commit=1;}
        if(k==0x67)selected=(selected+2)%3;if(k==0x6c)selected=(selected+1)%3;
        if(commit){unsigned ok=load?sango_save_load_slot(selected):sango_save_write_slot(selected);if(ok){restore(saved);return selected;}}
    }
}
static uint8_t *map_state;
static int clamp_coordinate(int value,int min,int max){
    if(value<min)return min;
    if(value>max)return max;
    return value;
}
enum {STRATEGY_TILE=40, STRATEGY_ROWS=9, STRATEGY_COLS=12, STRATEGY_HEIGHT=240};
void h1_strategy_hud(void){
    box(0,240,480,32,0x3020);char name[32];if(!ruler_portrait(g_scenario_id_view2,g_player_character,4,244,24))sprite(g_scenario_id_view2+47,g_player_character,4,244,24,24);sango_load_named_text(g_player_character,name);h1_native_text(34,249,name,14,0xff16);
    number(108,249,g_current_year,0xff16);h1_native_text(140,249,"年",14,0xff16);number(166,249,g_current_month,0xff16);h1_native_text(187,249,"月",14,0xff16);
    if(map_state){uint8_t *cities=(uint8_t *)sango_resource_item(2,14,g_resource_package_address);unsigned city=cities[map_state[3]*12+map_state[2]];if(city){sango_city_read_name(city-1,name);h1_native_text(218,249,name,14,0xff16);}}
    button(298,243,55,26,"信息",0);button(358,243,55,26,"查找",0);button(418,243,58,26,"菜单",0);
}
uint32_t sango_strategy_draw_map(uint8_t *state,uint8_t scroll){
    (void)scroll;h1_screen_id=13;map_state=state;h1_native_active=1;h1_layout_game=1;state[0]=0;
    if(state[1]!=strategy_y/STRATEGY_TILE)strategy_y=clamp_coordinate(state[1]*STRATEGY_TILE,0,120);
    if(state[3]<strategy_y/STRATEGY_TILE)strategy_y=state[3]*STRATEGY_TILE;
    if(state[3]>(strategy_y+STRATEGY_HEIGHT-1)/STRATEGY_TILE)
        strategy_y=(state[3]+1)*STRATEGY_TILE-STRATEGY_HEIGHT;
    state[1]=strategy_y/STRATEGY_TILE;
    uint8_t *terrain=(uint8_t *)sango_resource_item(54,1,g_resource_package_address),*cities=(uint8_t *)sango_resource_item(2,14,g_resource_package_address);
    if(!strategy_terrain){
        strategy_terrain=h1_alloc(480*STRATEGY_ROWS*STRATEGY_TILE*sizeof(*strategy_terrain));
        if(strategy_terrain){
            for(int row=0;row<STRATEGY_ROWS;row++){
                for(int col=0;col<STRATEGY_COLS;col++){
                    unsigned index=row*STRATEGY_COLS+col;
                    blit_sprite(strategy_terrain,STRATEGY_ROWS*STRATEGY_TILE,terrain+6+index*256,32,32,0,col*40,row*40,40,40);
                }
            }
        }
    }
    if(strategy_terrain)memcpy(h1_native_pixels,strategy_terrain+strategy_y*480,480*STRATEGY_HEIGHT*sizeof(*strategy_terrain));
    int last_row=(strategy_y+STRATEGY_HEIGHT-1)/STRATEGY_TILE;
    for(int row=state[1];row<=last_row;row++)for(int col=0;col<STRATEGY_COLS;col++){
        int y=row*STRATEGY_TILE-strategy_y;
        unsigned index=row*STRATEGY_COLS+col;
        if(!strategy_terrain)sprite_pixels(terrain+6+index*256,32,32,0,col*40,y,40,40);
        unsigned city=cities[index];if(city){unsigned owner=(&g_city_owner_one_based_view)[city*36];sprite(55,owner==g_player_character+1?8:owner?7:0,col*40+8,y+8,24,24);}
    }
    int cx=state[2]*40,cy=state[3]*40-strategy_y;
    frame(cx+1,cy+1,38,38,0xff80);frame(cx+2,cy+2,36,36,0xff80);
    h1_strategy_hud();h1_native_flip();return cities[state[3]*12+state[2]];
}
void sango_strategy_present(void){h1_native_active=1;h1_native_flip();}
uint32_t sango_strategy_select_city(uint8_t *s){
    int x=0,y=0;unsigned selected=sango_strategy_draw_map(s,0);
    for(;;){int e[3];sango_input_next_event(e);int k=e[0]==5?e[1]:0;
        if(e[0]==SANGO_TOUCH_TAP){x=sango_touch_x(e);y=sango_touch_y(e);if(y<240){s[2]=x/40;s[3]=(strategy_y+y)/40;selected=sango_strategy_draw_map(s,0);if(selected)return selected-1;continue;}if(x>=418)return 255;if(x>=358){Saved saved=save_screen();page("天下形势");h1_screen_id=14;sprite(47,0,114,47,252,192);h1_native_flip();next(&x,&y);restore(saved);continue;}if(x>=298&&selected)sango_ui_show_city_details(selected-1);continue;}
        if(e[0]==SANGO_TOUCH_PAN || e[0]==SANGO_TOUCH_PAN_END){
            if(!sango_touch_started_in_map(480,STRATEGY_HEIGHT))continue;
            int delta=(int16_t)((unsigned)e[1]>>16);
            int next_y=clamp_coordinate(strategy_y-delta,0,120);
            if(next_y!=strategy_y){
                strategy_y=next_y;
                s[1]=strategy_y/STRATEGY_TILE;
                s[3]=clamp_coordinate(s[3],s[1],(strategy_y+STRATEGY_HEIGHT-1)/STRATEGY_TILE);
                selected=sango_strategy_draw_map(s,0);
            }
            continue;
        }
        if(k==1)return 255;if(k==0x1c&&selected)return selected-1;
        if(k==0x69&&s[2])s[2]--;if(k==0x6a&&s[2]<11)s[2]++;if(k==0x67&&s[3])s[3]--;if(k==0x6c&&s[3]<8)s[3]++;
        if(k)selected=sango_strategy_draw_map(s,0);
    }
}
void h1_native_battle_hud(void){
    h1_ui_asset(6,0,216);h1_native_text(12,222,"粮",14,0xff16);number(36,222,g_battle_attacker_food,0xff16);h1_native_text(107,222,"敌粮",14,0xff16);number(147,222,g_battle_defender_food,0xff16);h1_native_text(223,222,"第",14,0xff16);number(246,222,g_battle_day,0xff16);h1_native_text(278,222,"天",14,0xff16);
    button(320,220,66,28,"信息",0);button(390,220,80,28,"返回",0);
    h1_native_text(16,250,"方向移动 · 确定行动 · 返回菜单",12,0xef57);
}
static int battle_x,battle_y;
int h1_battle_phase_x,h1_battle_phase_y;

void h1_battle_camera_reset(void){
    battle_x=0;
    battle_y=0;
    h1_battle_phase_x=0;
    h1_battle_phase_y=0;
}

static void battle_position(int x,int y){
    int max_x=(int)g_battle_map_width*H1_BATTLE_TILE-H1_BATTLE_WIDTH;
    int max_y=(int)g_battle_map_height*H1_BATTLE_TILE-H1_BATTLE_HEIGHT;
    battle_x=clamp_coordinate(x,0,max_x>0?max_x:0);
    battle_y=clamp_coordinate(y,0,max_y>0?max_y:0);
    h1_battle_phase_x=battle_x%H1_BATTLE_TILE;
    h1_battle_phase_y=battle_y%H1_BATTLE_TILE;
}

int h1_battle_pan(const int *event){
    if(!sango_touch_started_in_map(H1_BATTLE_WIDTH,H1_BATTLE_HEIGHT))return 0;
    int old_x=battle_x,old_y=battle_y;
    int dx=(int16_t)(unsigned)event[1],dy=(int16_t)((unsigned)event[1]>>16);
    battle_position(old_x-dx,old_y-dy);
    if(battle_x==old_x&&battle_y==old_y)return 0;
    int x=battle_x/H1_BATTLE_TILE,y=battle_y/H1_BATTLE_TILE;
    g_battle_cursor_x=clamp_coordinate(g_battle_cursor_x,x,(battle_x+H1_BATTLE_WIDTH-1)/H1_BATTLE_TILE);
    g_battle_cursor_y=clamp_coordinate(g_battle_cursor_y,y,(battle_y+H1_BATTLE_HEIGHT-1)/H1_BATTLE_TILE);
    if(x!=g_battle_viewport_x||y!=g_battle_viewport_y)sango_battle_draw_terrain(x,y);
    return 1;
}

int h1_battle_map_point(const int *event,uint8_t *state){
    int x=sango_touch_x(event),y=sango_touch_y(event);
    if((unsigned)x>=H1_BATTLE_WIDTH||(unsigned)y>=H1_BATTLE_HEIGHT)return 0;
    x=(battle_x+x)/H1_BATTLE_TILE;
    y=(battle_y+y)/H1_BATTLE_TILE;
    if(x>=g_battle_map_width||y>=g_battle_map_height)return 0;
    state[2]=x;
    state[3]=y;
    return 1;
}

void h1_battle_toolbar_point(int *event){
    int x=sango_touch_x(event),y=sango_touch_y(event);
    if(y<H1_BATTLE_HEIGHT)return;
    int legacy_x=x>=390?35:x>=320&&x<386?90:239;
    int legacy_y=x>=390?280:240;
    event[2]=legacy_x|(legacy_y<<16);
}

void sango_battle_copy_viewport_tiles(uint32_t x,uint32_t y){
    unsigned maxx=g_battle_map_width>20?g_battle_map_width-20:0,maxy=g_battle_map_height>9?g_battle_map_height-9:0;
    if(x>maxx)x=maxx;if(y>maxy)y=maxy;g_battle_viewport_x=x;g_battle_viewport_y=y;
    /* Keep one cached margin of tiles even when aligned. An LCD-pixel
     * movement inside this tile base then needs no terrain redraw. */
    unsigned map=sango_battle_resource_item(g_battle_map_resource,1);
    for(unsigned row=0;row<10;row++)for(unsigned col=0;col<21;col++)
        ((uint8_t *)g_viewport_tiles_address)[row*21+col]=(col+x<g_battle_map_width&&row+y<g_battle_map_height)?((uint8_t *)(map+16))[(row+y)*g_battle_map_width+col+x]:0;
}
void sango_battle_draw_terrain(uint32_t x,uint32_t y){
    sango_battle_copy_viewport_tiles(x,y);
    int bx=g_battle_viewport_x,by=g_battle_viewport_y;
    battle_position(battle_x/H1_BATTLE_TILE==bx?battle_x:bx*H1_BATTLE_TILE,
                    battle_y/H1_BATTLE_TILE==by?battle_y:by*H1_BATTLE_TILE);
    unsigned tiles=sango_battle_resource_item(g_battle_tileset_resource,1);
    for(unsigned row=0;row<10;row++)for(unsigned col=0;col<21;col++)
        sango_raster_draw_opaque(col*16,row*16,16,16,tiles+6+((uint8_t *)g_viewport_tiles_address)[row*21+col]*64,g_game_context);
}
void sango_battle_info_show(void){
    Saved saved=save_screen();unsigned unit=sango_battle_find_unit_at(g_battle_cursor_x,g_battle_cursor_y);
    char title[40],value[160];
    if(unit<20){unsigned who=sango_battle_unit_character(unit);SangoCharacter *c=(SangoCharacter *)(g_character_table_address+who*16);SangoBattleUnit *u=(SangoBattleUnit *)(&g_first_unit_x)+unit;
        sango_load_named_text(who,title);page(title);if(!ruler_portrait(g_scenario_id_view2,who,34,60,72))sprite(g_scenario_id_view2+47,who,34,60,72,72);
        const char *labels[]={"等级","兵力","武力","智力","生命","技能","体力","忠诚"};unsigned values[]={c->level,c->troops,c->strength,c->intelligence,u->life,u->energy,c->stamina,c->loyalty};
        for(int i=0;i<8;i++){int x=142+(i&1)*156,y=62+(i/2)*37;txt(x,y,labels[i]);number(x+70,y,values[i],0x2983);}
    }else{page("地形信息");h1_screen_id=15;unsigned terrain=sango_battle_terrain_at(g_battle_cursor_x,g_battle_cursor_y);if(terrain<8){sango_battle_read_text(terrain+14,value);paragraph(36,64,408,value);}}
    foot("点击或按键返回战场");h1_native_flip();int x,y;next(&x,&y);restore(saved);
}
void sango_battle_show_overview(void){
    Saved saved=save_screen();page("战场全图");h1_screen_id=16;unsigned w=g_battle_map_width,h=g_battle_map_height;int size=432/w;if(size>(int)(180/h))size=180/h;if(size<1)size=1;int ox=(480-w*size)/2,oy=48;
    unsigned map=sango_battle_resource_item(g_battle_map_resource,1),tiles=sango_battle_resource_item(g_battle_tileset_resource,1);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)sprite_pixels((uint8_t *)(tiles+6+((uint8_t *)(map+16))[y*w+x]*64),16,16,0,ox+x*size,oy+y*size,size,size);
    for(int i=0;i<20;i++)if((&g_first_unit_status)[i*7]!=SANGO_UNIT_RETIRED)box(ox+(&g_first_unit_x)[i*7]*size,oy+(&g_first_unit_y)[i*7]*size,size,size,i<10?0xf800:0x001f);
    foot("点击或按键返回战场");h1_native_flip();int x,y;next(&x,&y);restore(saved);
}
