/* H1/Y100 V1.41 backend. Recovered engine coordinates stay in owned software
 * surfaces; only this file accesses native H1 services. */
#include "h1_sdk.h"
#include "sango_internal.h"
#include "string.h"
#include "font.inc"
#include "battle_art.inc"

unsigned char h1_private_stack[65536] __attribute__((section(".h1_noinit.stack"),aligned(16)));
unsigned char sango_legacy_blob[0xbc20] __attribute__((aligned(16)));
extern unsigned char sango_original_data[];
extern unsigned int sango_original_data_size;
int h1_layout_game;
volatile unsigned int h1_diagnostic[16];
enum {SURFACE_HEIGHT=320, BATTLE_SOURCE_HEIGHT=160,
      BATTLE_SURFACE_WIDTH=H1_SURFACE_WIDTH*3/2, BATTLE_SURFACE_HEIGHT=BATTLE_SOURCE_HEIGHT*3/2,
      BATTLE_SURFACE_BYTES=BATTLE_SURFACE_WIDTH*BATTLE_SURFACE_HEIGHT*sizeof(uint16_t)};
typedef struct {
    uint16_t pixels[H1_SURFACE_WIDTH*SURFACE_HEIGHT];
    unsigned fill,text,font,mode;
    int x,y;
    uint16_t *battle_pixels;
} Surface;
static Surface window;
static int guard,dirty,closed;
static unsigned present_tick;
static unsigned timers[8],due[8];
static uint16_t pen_x=65535,pen_y=65535;
static int pen_down,pen_button,raw_down,press_pending;
static unsigned press_tick;
static uint16_t last_raw_x=65535,last_raw_y=65535;
static unsigned queued[3],has_queued;
static unsigned char held[6];
static void *gui(unsigned offset){return h1_runtime_entry(h1_runtime_table(H1_RUNTIME_GUI_TABLE_SLOT),offset);}
static void *fs(unsigned offset){return h1_runtime_entry(h1_runtime_table(H1_RUNTIME_FS_TABLE_SLOT),offset);}

void h1_run_game_sessions(void (*run)(void))
{
#ifdef H1_POWER_H
    h1_power_run(run);
#else
    run();
#endif
}

static Surface *surface(unsigned context){return context>0x80000000u?(Surface *)context:&window;}
static unsigned rgb(unsigned r,unsigned g,unsigned b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static unsigned object(unsigned o){switch(o){case 15:return 65535;case 8:return 0xad55;case 7:return 0x528a;case 16:return 0;default:return o;}}
static int battle_scale(int position){return (position*3+1)/2;}

static void battle_legacy_pixel(uint16_t *detail,int x,int y,uint16_t color)
{
    int left=battle_scale(x),right=battle_scale(x+1);
    int top=battle_scale(y),bottom=battle_scale(y+1);
    for(int row=top;row<bottom;row++)
        for(int col=left;col<right;col++)detail[row*BATTLE_SURFACE_WIDTH+col]=color;
}

static uint16_t *battle_surface(Surface *s)
{
    if(s->battle_pixels)return s->battle_pixels;
    uint16_t *detail=h1_alloc(BATTLE_SURFACE_BYTES);
    if(!detail)return 0;
    for(int y=0;y<BATTLE_SOURCE_HEIGHT;y++)for(int x=0;x<H1_SURFACE_WIDTH;x++)
        battle_legacy_pixel(detail,x,y,s->pixels[y*H1_SURFACE_WIDTH+x]);
    s->battle_pixels=detail;
    return detail;
}

static void pixel(Surface *s,int x,int y,unsigned c)
{
    if((unsigned)x>=H1_SURFACE_WIDTH||(unsigned)y>=SURFACE_HEIGHT)return;
    s->pixels[y*H1_SURFACE_WIDTH+x]=c;
    if(s->battle_pixels&&y<BATTLE_SOURCE_HEIGHT)battle_legacy_pixel(s->battle_pixels,x,y,c);
    if(s==&window)dirty=1;
}

static void sample_battle_cell(Surface *s,int x,int y,const uint16_t *cell)
{
    for(int row=0;row<16;row++)for(int col=0;col<16;col++)
        s->pixels[(y+row)*H1_SURFACE_WIDTH+x+col]=cell[battle_scale(row)*BATTLE_SURFACE_WIDTH+battle_scale(col)];
    if(s==&window)dirty=1;
}

static uint16_t *battle_cell(Surface *s,int x,int y)
{
    if(h1_layout_game!=2||!sango_color_is_loaded()||
       (unsigned)x>H1_SURFACE_WIDTH-16||(unsigned)y>BATTLE_SOURCE_HEIGHT-16)
        return 0;
    uint16_t *detail=battle_surface(s);
    return detail?detail+battle_scale(y)*BATTLE_SURFACE_WIDTH+battle_scale(x):0;
}

int h1_battle_tile_art(unsigned tile,int x,int y,unsigned target)
{
    if(tile>=sizeof(battle_terrain_tiles)/sizeof(*battle_terrain_tiles))return 0;
    Surface *s=surface(target);
    uint16_t *cell=battle_cell(s,x,y);
    if(!cell)return 0;
    for(int row=0;row<H1_BATTLE_TILE;row++)
        memcpy(cell+row*BATTLE_SURFACE_WIDTH,battle_terrain_tiles[tile]+row*H1_BATTLE_TILE,
               H1_BATTLE_TILE*sizeof(*cell));
    sample_battle_cell(s,x,y,cell);
    return 1;
}

uint16_t h1_blend_rgb565(unsigned fg,unsigned bg,unsigned weight)
{
    unsigned rest=255-weight;
    unsigned red=((fg>>11)*weight+(bg>>11)*rest+127)/255;
    unsigned green=(((fg>>5)&63)*weight+((bg>>5)&63)*rest+127)/255;
    unsigned blue=((fg&31)*weight+(bg&31)*rest+127)/255;
    return (red<<11)|(green<<5)|blue;
}

int h1_battle_unit_art(unsigned unit,int x,int y,unsigned target)
{
    if(unit>=sizeof(battle_unit_colors)/sizeof(*battle_unit_colors))return 0;
    Surface *s=surface(target);
    uint16_t *cell=battle_cell(s,x,y);
    if(!cell)return 0;
    for(int row=0;row<H1_BATTLE_TILE;row++)for(int col=0;col<H1_BATTLE_TILE;col++){
        int index=row*H1_BATTLE_TILE+col;
        unsigned alpha=battle_unit_alpha[unit][index];
        uint16_t *dest=cell+row*BATTLE_SURFACE_WIDTH+col;
        if(alpha)*dest=alpha==255?battle_unit_colors[unit][index]:
            h1_blend_rgb565(battle_unit_colors[unit][index],*dest,alpha);
    }
    sample_battle_cell(s,x,y,cell);
    return 1;
}
static void fill(Surface *s,int x,int y,int w,int h,unsigned c){
    if(w<=0||h<=0)return;int r=x+w,b=y+h;if(x<0)x=0;if(y<0)y=0;if(r>H1_SURFACE_WIDTH)r=H1_SURFACE_WIDTH;if(b>SURFACE_HEIGHT)b=SURFACE_HEIGHT;
    for(int j=y;j<b;j++)for(int i=x;i<r;i++)pixel(s,i,j,c);
}
static int glyph(unsigned code){int lo=0,hi=sizeof(font_codes)/sizeof(*font_codes);while(lo<hi){int m=(lo+hi)/2;if(font_codes[m]<code)lo=m+1;else hi=m;}return lo<(int)(sizeof(font_codes)/sizeof(*font_codes))&&font_codes[lo]==code?lo:0;}
static void text(Surface *s,int x,int y,const char *str,int count){
    int size=s->font==8?24:s->font==4?16:12;int ox=x;
    const unsigned char *p=(const unsigned char *)str;
    while(*p&&count!=0){unsigned code=*p++;if(count>0)--count;
        if(code=='\n'){x=ox;y+=size;continue;}
        if(code>127&&*p){code=(code<<8)|*p++;if(count>0)--count;}
        int index=glyph(code),width=code<128?size/2:size;
        const unsigned char *bits=size==12?font12[index]:size==16?font16[index]:font24[index];
        if(!s->mode)fill(s,x,y-H1_FONT_TOP_PAD,width,size+H1_FONT_EXTRA_ROWS,s->fill);
        int stride=(size+7)/8;
        for(int j=0;j<size+H1_FONT_EXTRA_ROWS;j++)for(int i=0;i<width;i++)if(bits[j*stride+i/8]&(128>>(i&7)))pixel(s,x+i,y+j-H1_FONT_TOP_PAD,s->text);
        x+=width;
    }
}
static void panel_text(volatile uint32_t *out,int x,int y,const char *label){
    const unsigned char *p=(const unsigned char *)label;
    while(*p){unsigned code=*p++;if(code>127&&*p)code=(code<<8)|*p++;int n=glyph(code),w=code<128?8:16;
        for(int j=0;j<16;j++)for(int i=0;i<w;i++)if(x+i<480&&y+j<272&&(font16[n][j*2+i/8]&(128>>(i&7))))out[(y+j)*480+x+i]=0xffe6b5;
        x+=w;
    }
}
static void rect32(volatile uint32_t *out,int x,int y,int w,int h,unsigned c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)out[j*480+i]=c;}
static const unsigned button_keys[6]={0x67,0x6c,0x69,0x6a,0x1c,1};
void h1_compose_battle(void){
    if(window.battle_pixels){
        const uint16_t *detail=window.battle_pixels+h1_battle_phase_y*BATTLE_SURFACE_WIDTH+h1_battle_phase_x;
        for(int y=0;y<H1_BATTLE_HEIGHT;y++)
            memcpy(h1_native_pixels+y*480,detail+y*BATTLE_SURFACE_WIDTH,480*sizeof(*detail));
    }else for(int y=0;y<H1_BATTLE_HEIGHT;y++){
        const uint16_t *row=window.pixels+((y+h1_battle_phase_y)*2/3)*H1_SURFACE_WIDTH;
        uint16_t *out=h1_native_pixels+y*480;
        int src=h1_battle_phase_x;
        /* Three LCD pixels for every two legacy pixels, preserving the
         * nearest-neighbor sampling phase as the camera moves one pixel. */
        for(int x=0;x<480;x+=3,src+=3){
            out[x]=row[src*2/3];
            out[x+1]=row[(src+1)*2/3];
            out[x+2]=row[(src+2)*2/3];
        }
    }
    h1_native_battle_hud();
}
static void present(int force){
    unsigned now=h1_raw_tick_80hz();
    if(!dirty||guard||(!force&&(unsigned)(now-present_tick)<2))return;
    typedef volatile uint32_t *(*Get)(uint16_t *,void *);
    uint16_t info[6]={0};volatile uint32_t *out=((Get)gui(0x8f4))(info,0);
    if(!out||info[1]!=480||info[2]!=272||info[3]!=1920||info[5]!=32){h1_diagnostic[2]=1;return;}
    if(!h1_native_active&&h1_layout_game==2)h1_compose_battle();
    for(int y=0;y<272;y++)for(int x=0;x<480;x++){
        unsigned p=h1_native_pixels[y*480+x];
        out[y*480+x]=((p&0xf800)<<8)|((p&0x7e0)<<5)|((p&31)<<3);
    }
    present_tick=now;dirty=0;++h1_diagnostic[3];
}
__attribute__((noinline)) int h1_map_touch(int x,int y,int *sx,int *sy){
    if((unsigned)x>=480||(unsigned)y>=272)return 0;
    *sx=x;*sy=y;return 1;
}
static int map_key(int key){switch(key){case 37:return 0x67;case 27:return 0x6c;case 40:case 26:return 0x69;case 35:case 28:return 0x6a;case 39:case 25:case 23:return 0x1c;case 24:case 41:return 1;default:return 0;}}
static int poll(unsigned *event){
    present(1);
    if(has_queued){for(int i=0;i<3;i++)event[i]=queued[i];has_queued=0;return 1;}
    int code,value;
    h1_event_fetch(&code,&value);
    if(code==11&&!raw_down){raw_down=1;press_pending=1;press_tick=h1_raw_tick_80hz();}
    else if(code==8)raw_down=0;
    if(code==9||code==10){int k=map_key(value);for(int i=0;i<6;i++)if(button_keys[i]==(unsigned)k)held[i]=code==9;
        if(k&&code==9){event[0]=0x10;event[1]=k;event[2]=0;return 1;}}
    typedef void (*Pen)(uint16_t *,uint16_t *);uint16_t px=65535,py=65535;
    ((Pen)gui(0x6c0))(&px,&py);
    /* H1's DOWN packet can precede the coordinate getter update. Do not use
     * the preceding contact as a new drag origin. Wait two native ticks;
     * contacts released sooner become one down/up pair at the final point. */
    if(press_pending){
        /* Lists and the strategy map sample settled drags. Other native
         * menus retain release-position taps. */
        unsigned elapsed=h1_raw_tick_80hz()-press_tick;
        if(raw_down&&((h1_native_active&&h1_screen_id!=13&&!h1_list_touch_active)||
            elapsed<2||(px==last_raw_x&&py==last_raw_y&&(!h1_list_touch_active||elapsed<4))))return 0;
        if(px<480&&py<272){
            int tx,ty;if(h1_map_touch(px,py,&tx,&ty)){
                press_pending=0;
                if(!raw_down){
                    last_raw_x=px;last_raw_y=py;
                    pen_x=tx;pen_y=ty;event[0]=1;event[1]=0;event[2]=tx|(ty<<16);
                    queued[0]=2;queued[1]=0;queued[2]=event[2];has_queued=1;return 1;
                }
            }
        }else if(!raw_down)press_pending=0;
    }
    int down=raw_down&&px<480&&py<272,sx=0,sy=0;
    if(down){
        int mapped=h1_map_touch(px,py,&sx,&sy);
        if(!pen_down){pen_button=-1;
            pen_down=1;
            if(pen_button>=0){event[0]=0x10;event[1]=button_keys[pen_button];event[2]=0;return 1;}
            if(mapped){pen_x=sx;pen_y=sy;event[0]=1;event[1]=0;event[2]=sx|(sy<<16);return 1;}
        }else if(pen_button<0&&mapped){pen_x=sx;pen_y=sy;}
    }else if(pen_down){
        last_raw_x=px;last_raw_y=py;pen_down=0;
        if(pen_button<0){
            if(code==8&&h1_list_touch_active&&h1_map_touch(px,py,&sx,&sy)){pen_x=sx;pen_y=sy;}
            event[0]=2;event[1]=0;event[2]=pen_x|(pen_y<<16);return 1;
        }
        pen_button=-1;
    }
    unsigned now=h1_raw_tick_80hz();
    for(int i=1;i<8;i++)if(timers[i]&&(int)(now-due[i])>=0){due[i]=now+timers[i];event[0]=0x144;event[1]=i;event[2]=0;return 1;}
    return 0;
}
void *memset(void *p,int c,size_t n){unsigned char *d=p;while(n--)*d++=c;return p;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *h1_compat_memset(void *p,int c,unsigned n){return memset(p,c,n);}
void h1_app_main(void){
    memset(&window,0,sizeof(window));window.fill=65535;window.text=0;closed=0;guard=0;dirty=1;h1_layout_game=0;
    memset(timers,0,sizeof(timers));memset(held,0,sizeof(held));has_queued=pen_down=raw_down=press_pending=0;pen_button=-1;
    last_raw_x=last_raw_y=65535;
    memset(sango_legacy_blob,0,sizeof(sango_legacy_blob));memcpy(sango_legacy_blob,sango_original_data,sango_original_data_size);
    for(unsigned i=0;i+4<=sango_original_data_size;i+=4){unsigned *v=(unsigned *)(sango_legacy_blob+i);if(*v>=0x81c1fef0u&&*v<0x81c2bb00u)*v=(unsigned)sango_legacy_blob+*v-0x81c1fef0u;}
    h1_native_active=1;h1_diagnostic[0]=0x53474831;sango_main();h1_ui_assets_free();h1_diagnostic[1]=9;
    if(window.battle_pixels)h1_free(window.battle_pixels);
    window.battle_pixels=0;
    typedef void (*Close)(void);((Close)gui(0x850))();
}
int bda_sango_gui_shell_open(unsigned a){(void)a;typedef int(*Open)(void);return ((Open)gui(0x84c))();}
int bda_sango_gui_register_frame(void *d){(void)d;sango_window_proc(1,0x60,0,0);return 1;}
unsigned bda_sango_gui_current_draw(unsigned a){(void)a;return (unsigned)&window;}
unsigned bda_sango_gui_object_draw_begin(unsigned a){(void)a;return (unsigned)&window;}
void bda_sango_gui_object_draw_end(unsigned a,unsigned b){(void)a;(void)b;present(0);}
int bda_sango_gui_draw_guard(unsigned a){if(a)guard++;else if(guard)guard--;if(!guard)present(0);return 1;}
int bda_sango_gui_request_redraw(unsigned a,unsigned b,unsigned c){(void)a;(void)b;(void)c;dirty=1;present(0);return 1;}
void *bda_sango_mem_alloc(unsigned n){return h1_alloc(n);}
void bda_sango_mem_free(void *p){h1_free(p);}
void *bda_sango_gui_compatible_create(unsigned a){(void)a;Surface *s=h1_alloc(sizeof(Surface));if(s){memset(s,0,sizeof(*s));s->fill=65535;}return s;}
void bda_sango_gui_compatible_free(void *p){
    Surface *s=p;
    if(s&&s!=&window){if(s->battle_pixels)h1_free(s->battle_pixels);h1_free(s);}
}
void *bda_sango_gui_draw_object(unsigned a){return (void *)(object(a)+0x10000u);}
static unsigned color_object(unsigned a){return a>=0x10000&&a<0x20000?a-0x10000:a;}
unsigned bda_sango_gui_set_fill_object(unsigned c,unsigned o){Surface *s=surface(c);unsigned old=s->fill;s->fill=color_object(o);return old;}
unsigned bda_sango_gui_bind_image_resource(unsigned c,unsigned o){return bda_sango_gui_set_fill_object(c,o);}
int bda_sango_gui_set_text_color(unsigned c,unsigned o){Surface *s=surface(c);unsigned old=s->text;s->text=color_object(o);return old;}
int bda_sango_gui_set_text_mode(unsigned c,unsigned m){Surface *s=surface(c);int old=s->mode;s->mode=m;return old;}
int bda_sango_gui_rgb(unsigned c,unsigned r,unsigned g,unsigned b){(void)c;return rgb(r,g,b);}
int bda_sango_gui_fill_region(unsigned c,int x,int y,int w,int h){Surface *s=surface(c);fill(s,x,y,w,h,s->fill);return 1;}
int bda_sango_gui_put_pixel(unsigned c,int x,int y,unsigned p){pixel(surface(c),x,y,p);return 1;}
int bda_sango_gui_select_font(unsigned c,unsigned f){Surface *s=surface(c);int old=s->font;s->font=f;return old;}
void *bda_sango_gui_current_font(unsigned a){return (void *)a;}
int bda_sango_gui_font_cell_width(unsigned c){return surface(c)->font==8?24:surface(c)->font==4?16:12;}
int bda_sango_gui_font_cell_height(unsigned c){return bda_sango_gui_font_cell_width(c);}
int bda_sango_gui_draw_text(unsigned c,int x,int y,const char *t,int n){text(surface(c),x,y,t,n);return 1;}
void *bda_sango_gui_select_draw_object(unsigned c,void *p){return (void *)(bda_sango_gui_set_fill_object(c,(unsigned)p)+0x10000);}
void *bda_gui_select_draw_object(unsigned c,void *p){return bda_sango_gui_select_draw_object(c,p);}
void bda_sango_gui_move_to(unsigned c,int x,int y){surface(c)->x=x;surface(c)->y=y;}
void bda_sango_gui_line_to(unsigned c,int x,int y){Surface *s=surface(c);int a=s->x,b=s->y,dx=x>a?x-a:a-x,dy=y>b?b-y:y-b,ix=a<x?1:-1,iy=b<y?1:-1,e=dx+dy;while(1){pixel(s,a,b,s->fill);if(a==x&&b==y)break;int t=2*e;if(t>=dy){e+=dy;a+=ix;}if(t<=dx){e+=dx;b+=iy;}}s->x=x;s->y=y;}
void bda_sango_gui_rectangle(unsigned c,int l,int t,int r,int b){bda_sango_gui_move_to(c,l,t);bda_sango_gui_line_to(c,r,t);bda_sango_gui_line_to(c,r,b);bda_sango_gui_line_to(c,l,b);bda_sango_gui_line_to(c,l,t);}
void bda_sango_gui_circle(unsigned c,int x,int y,int r){int a=r,b=0,e=1-r;while(a>=b){int xx[8]={a,b,-b,-a,-a,-b,b,a},yy[8]={b,a,a,b,-b,-a,-a,-b};for(int i=0;i<8;i++)pixel(surface(c),x+xx[i],y+yy[i],surface(c)->fill);b++;if(e<0)e+=2*b+1;else{a--;e+=2*(b-a)+1;}}}
int bda_sango_gui_draw_aux_rectangle(unsigned c,int x,int y,int w,int h){Surface *s=surface(c);for(int j=y;j<=y+h;j++)for(int i=x;i<=x+w;i++)if((unsigned)i<H1_SURFACE_WIDTH&&(unsigned)j<SURFACE_HEIGHT)pixel(s,i,j,s->pixels[j*H1_SURFACE_WIDTH+i]^65535);return 1;}
int bda_sango_gui_context_copy(unsigned src,int sx,int sy,int w,int h,unsigned dst,int dx,int dy,unsigned key){
    Surface *a=surface(src),*b=surface(dst);
    if(w==240&&h==320)w=H1_SURFACE_WIDTH;
    int detail_rows=0;
    /* The engine publishes and snapshots battle surfaces at the same origin. */
    if(a->battle_pixels&&sx==dx&&sy==dy&&(unsigned)sx<H1_SURFACE_WIDTH&&
       (unsigned)sy<BATTLE_SOURCE_HEIGHT&&w>0&&h>0){
        uint16_t *detail=battle_surface(b);
        if(detail){
            int right=sx+w>H1_SURFACE_WIDTH?H1_SURFACE_WIDTH:sx+w;
            detail_rows=sy+h>BATTLE_SOURCE_HEIGHT?BATTLE_SOURCE_HEIGHT-sy:h;
            for(int y=battle_scale(sy);y<battle_scale(sy+detail_rows);y++){
                int row=y*BATTLE_SURFACE_WIDTH;
                for(int x=battle_scale(sx);x<battle_scale(right);x++){
                    uint16_t p=a->battle_pixels[row+x];
                    if(p!=key)detail[row+x]=p;
                }
            }
        }
    }
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        if((unsigned)(sx+x)>=H1_SURFACE_WIDTH||(unsigned)(sy+y)>=SURFACE_HEIGHT)continue;
        unsigned p=a->pixels[(sy+y)*H1_SURFACE_WIDTH+sx+x];
        if(p==key)continue;
        if(y<detail_rows){
            b->pixels[(dy+y)*H1_SURFACE_WIDTH+dx+x]=p;
            if(b==&window)dirty=1;
        }else pixel(b,dx+x,dy+y,p);
    }
    return 1;
}
void bda_gui_render_picture(unsigned c,int x,int y,int w,int h,const bda_gui_picture_t *p){for(int j=0;j<h;j++)for(int i=0;i<w;i++)pixel(surface(c),x+i,y+j,p->source_pixels[(j*p->height/h)*p->width+i*p->width/w]);}
int bda_sango_gui_render_region(unsigned c,int x,int y,int w,int h,int sw,int sh,void *desc,int sx,int sy){
    unsigned *d=desc;const unsigned char *p=(void *)d[5];int bits=((unsigned char *)desc)[16];
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){int xx=sx+i,yy=sy+j;if((unsigned)xx>=(unsigned)sw||(unsigned)yy>=(unsigned)sh)continue;unsigned v;
        if(bits==16)v=((const uint16_t *)p)[yy*sw+xx];else{v=(p[yy*((sw+3)/4)+xx/4]>>(6-2*(xx&3)))&3;v=object(v==0?15:v==1?8:v==2?7:16);}
        if(d[6]!=v)pixel(surface(c),x+i,y+j,v);
    }return 1;}
int bda_sango_gui_bitmap_context(unsigned c,int x,int y,int w,int h,const void *p,unsigned m){(void)m;const unsigned char *b=p;for(int j=0;j<h;j++)for(int i=0;i<w;i++){unsigned v=(b[j*((w+3)/4)+i/4]>>(6-2*(i&3)))&3;pixel(surface(c),x+i,y+j,object(v==0?15:v==1?8:v==2?7:16));}return 1;}
int bda_sango_gui_bitmap_buffer(int x,int y,int w,int h,const void *p,void *out,unsigned m){(void)x;(void)y;(void)m;memcpy(out,p,((w+3)/4)*h);return 1;}
int bda_sango_gui_bitmap_crop(int sw,int sh,int x,int y,int w,int h,const void *src,void *dst){(void)sh;const unsigned char *a=src;unsigned char *b=dst;memset(b,0,((w+3)/4)*h);for(int j=0;j<h;j++)for(int i=0;i<w;i++){unsigned v=(a[(y+j)*((sw+3)/4)+(x+i)/4]>>(6-2*((x+i)&3)))&3;b[j*((w+3)/4)+i/4]|=v<<(6-2*(i&3));}return 1;}
int bda_sango_gui_capture_region(int x,int y,int w,int h,void *p){uint16_t *d=p;for(int j=0;j<h;j++)for(int i=0;i<w;i++)*d++=(unsigned)(x+i)<H1_SURFACE_WIDTH&&(unsigned)(y+j)<SURFACE_HEIGHT?window.pixels[(y+j)*H1_SURFACE_WIDTH+x+i]:0;return 1;}
int bda_sango_gui_restore_region(int x,int y,int w,int h,const void *p){const uint16_t *d=p;for(int j=0;j<h;j++)for(int i=0;i<w;i++)pixel(&window,x+i,y+j,*d++);return 1;}
unsigned bda_gui_tick_count_25ms(void){return h1_raw_tick_80hz()/2;}
unsigned bda_gui_window_timer_clock_ms(void){return h1_raw_tick_80hz()*25/2;}
int bda_gui_window_timer_start(unsigned c,unsigned id,unsigned ms){(void)c;if(id>=8)return 0;timers[id]=(ms*80+999)/1000;if(!timers[id])timers[id]=1;due[id]=h1_raw_tick_80hz()+timers[id];return 1;}
int bda_gui_window_timer_stop(unsigned c,unsigned id){(void)c;if(id<8)timers[id]=0;return 1;}
int bda_sango_gui_timer_start(unsigned c,unsigned id,unsigned ms){return bda_gui_window_timer_start(c,id,ms);}
int bda_sango_gui_timer_stop(unsigned c,unsigned id){return bda_gui_window_timer_stop(c,id);}
void bda_gui_touch_position(uint16_t *x,uint16_t *y){*x=pen_x;*y=pen_y;}
int bda_sango_gui_input_packet(void *out){unsigned char *p=out;int map[6]={3,2,1,0,5,4};for(int i=0;i<6;i++)p[i]=held[map[i]];return 1;}
int bda_gui_event_pump_frame_once(bda_gui_message_t *m,unsigned frame){unsigned e[3];(void)m;if(!poll(e))return 0;sango_window_proc(frame,e[0],e[1],e[2]);return 1;}
int bda_sango_gui_event_poll(void *p,unsigned a){(void)a;if(closed)return 0;unsigned e[3];while(!poll(e)){}memcpy(p,e,12);return 1;}
int bda_sango_gui_event_step(void *p){(void)p;return 1;}
int bda_sango_gui_event_dispatch(void *p){unsigned *e=p;sango_window_proc(1,e[0],e[1],e[2]);return 1;}
int bda_sango_gui_post_message(unsigned a,unsigned b,unsigned c,unsigned d){(void)a;queued[0]=b;queued[1]=c;queued[2]=d;has_queued=1;return 1;}
int bda_sango_gui_send_message(unsigned a,unsigned b,unsigned c,unsigned d){return sango_window_proc(a,b,c,d);}
int bda_sango_gui_default_proc(unsigned a,unsigned b,unsigned c,unsigned d){(void)a;(void)b;(void)c;(void)d;return 0;}
int bda_sango_gui_frame_stop(unsigned a){(void)a;closed=1;return 1;}
int bda_sango_gui_frame_release(unsigned a){(void)a;return 1;}
void bda_sango_gui_close_frame(unsigned a){(void)a;closed=1;}
void bda_sango_gui_end_draw(unsigned a){(void)a;}
int bda_sango_gui_window_aux_a(unsigned a,unsigned b,unsigned c){(void)a;(void)b;(void)c;return 1;}
int bda_sango_gui_window_aux_b(unsigned a,unsigned b,unsigned c){return bda_sango_gui_window_aux_a(a,b,c);}
int bda_sango_gui_display_metric(unsigned a,unsigned b){(void)b;return a?320:240;}
unsigned bda_sango_gui_control_create(const char *a,const char *b,unsigned c,unsigned d,unsigned e,int f,int g,int h,int i,unsigned j,unsigned k){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k;return 0;}
int bda_sango_gui_control_destroy(unsigned a){(void)a;return 0;}
int bda_sango_gui_set_active(unsigned a){(void)a;return 1;}
void bda_sango_gui_edit_finish(void){}
int bda_sango_gui_message_box(unsigned a,const char *message,const char *title,unsigned flags){
    (void)a;(void)flags;Surface *saved=bda_sango_gui_compatible_create(0);
    if(saved){
        memcpy(saved,&window,offsetof(Surface,battle_pixels));
        if(window.battle_pixels&&battle_surface(saved))
            memcpy(saved->battle_pixels,window.battle_pixels,BATTLE_SURFACE_BYTES);
    }
    fill(&window,8,80,224,100,0xdecf);window.font=4;window.text=0;window.mode=1;text(&window,16,88,title,-1);window.font=0;text(&window,16,112,message,-1);text(&window,32,156,"\xc8\xb7\xb6\xa8 / \xb7\xb5\xbb\xd8",-1);dirty=1;
    unsigned e[3];int result=0;while(1)if(poll(e)&&e[0]==0x10){if(e[1]==0x1c){result=1;break;}if(e[1]==1)break;}
    if(saved){
        memcpy(&window,saved,offsetof(Surface,battle_pixels));
        if(saved->battle_pixels)memcpy(window.battle_pixels,saved->battle_pixels,BATTLE_SURFACE_BYTES);
        else if(window.battle_pixels){h1_free(window.battle_pixels);window.battle_pixels=0;}
        bda_sango_gui_compatible_free(saved);
    }
    dirty=1;return result;
}
/* Resources, settings and paired saves share the application's game-data
 * directory. Firmware paths use GBK; serialized save bytes remain unchanged. */
#define H1_GAME_DIRECTORY "A:\\\xd3\xa6\xd3\xc3\\\xca\xfd\xbe\xdd\\\xd3\xce\xcf\xb7"
static const char *path(const char *p,char *out){
    unsigned n=0,start=0;for(unsigned i=0;p[i];i++)if(p[i]=='\\'||p[i]=='/')start=i+1;
    const char *name=p+start;int music=0;for(unsigned i=0;p[i];i++)if(p[i]=='-'&&p[i+1]=='m')music=1;
    const char *prefix=H1_GAME_DIRECTORY "\\";
    while(*prefix)out[n++]=*prefix++;
    if(music){const char *m="SGMUSIC\\";while(*m)out[n++]=*m++;}
    while(*name&&n<250)out[n++]=*name++;out[n]=0;return out;
}
void *bda_sango_fs_fopen(const char *p,const char *m){char mapped[256],mode[4];unsigned n=0;for(;*m&&n<3;m++)if(*m!='f')mode[n++]=*m;mode[n]=0;return h1_fopen(path(p,mapped),mode);}
int bda_sango_fs_fclose(void *p){return p?h1_fclose(p):-1;}
unsigned bda_sango_fs_fread(void *p,unsigned s,unsigned n,void *f){return h1_fread(p,s,n,f);}
unsigned bda_sango_fs_fwrite(const void *p,unsigned s,unsigned n,void *f){return h1_fwrite(p,s,n,f);}
int bda_sango_fs_fseek(void *p,int o,int w){return h1_fseek(p,o,w)<0?-1:0;}
int bda_sango_fs_ftell(void *p){return h1_ftell(p);}
int bda_sango_fs_feof(void *p){int pos=h1_ftell(p);int end=h1_fseek(p,0,2);h1_fseek(p,pos,0);return pos>=end;}
int bda_sango_fs_ferror(void *p){(void)p;return 0;}
void bda_sango_fs_clearerr(void *p){(void)p;}
int bda_sango_fs_rename(const char *a,const char *b){char aa[256],bb[256];typedef int(*Fn)(const char *,const char *);return ((Fn)fs(0x28))(path(a,aa),path(b,bb));}
int bda_sango_fs_mkdir(const char *a){(void)a;typedef int(*Fn)(const char *);return ((Fn)fs(0x30))(H1_GAME_DIRECTORY);}
unsigned bda_sango_fs_read_at(void *p,unsigned o,unsigned n,void *f){h1_fseek(f,o,0);return h1_fread(p,1,n,f);}
int bda_sango_fs_access(const char *p,int m){(void)m;if(!p)return -1;void *f=bda_sango_fs_fopen(p,"rb");if(!f)return -1;h1_fclose(f);return 0;}
int bda_fs_fopen_raw(const char *p,const char *m){return (int)bda_sango_fs_fopen(p,m);}
int bda_fs_file_is_valid(int f){return f!=0&&f!=-1;}
int bda_fs_close_raw(int f){return bda_sango_fs_fclose((void *)f);}
int bda_fs_read_raw(int f,void *p,unsigned n){return h1_fread(p,1,n,(void *)f);}
int bda_fs_write_raw(int f,const void *p,unsigned n){return h1_fwrite(p,1,n,(void *)f);}
int bda_fs_seek_raw(int f,int o,int w){return h1_fseek((void *)f,o,w);}
void bda_fs_flush_all(void){}
void h1_native_flip(void){dirty=1;present(1);}
void h1_native_text(int x,int y,const char *label,int size,unsigned color){
    const unsigned char *p=(const unsigned char *)label;int ox=x;
    while(*p){unsigned code=*p++;if(code=='\n'){x=ox;y+=size+4;continue;}if(code>127&&*p)code=(code<<8)|*p++;
        int n=glyph(code),w=code<128?size/2:size,stride=(size+7)/8;const unsigned char *bits=size==24?font24[n]:size==16?font16[n]:size==14?font14[n]:font12[n];
        for(int j=0;j<size+H1_FONT_EXTRA_ROWS;j++)for(int i=0;i<w;i++)if((unsigned)(x+i)<480&&(unsigned)(y+j-H1_FONT_TOP_PAD)<272&&(bits[j*stride+i/8]&(128>>(i&7))))h1_native_pixels[(y+j-H1_FONT_TOP_PAD)*480+x+i]=color;
        x+=w;
    }
}
