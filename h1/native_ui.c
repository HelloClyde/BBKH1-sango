/* Native 480x272 presentation. Gameplay state and save records remain upstream. */
#include "sango_internal.h"
#include "h1_sdk.h"
#include "string.h"
#include "sango_ruler_portraits.inc"
uint16_t h1_native_pixels[480*272];
int h1_native_active=1;
unsigned int h1_screen_id;
int h1_list_touch_active;
static unsigned char *art;
static uint16_t *strategy_terrain;
static int strategy_y;
enum {STRATEGY_TILE=40, STRATEGY_ROWS=9, STRATEGY_COLS=12, STRATEGY_HEIGHT=240,
      STRATEGY_WORLD_HEIGHT=STRATEGY_ROWS*STRATEGY_TILE, STRATEGY_CITY_SIZE=32,
      STRATEGY_BANNER_W=8, STRATEGY_BANNER_H=14, H1R_MAX_BYTES=2600000};
/* Keep panels and controls inside the paper's four cloud ornaments. */
enum {PAGE_LEFT=42, PAGE_RIGHT=438, PAGE_WIDTH=PAGE_RIGHT-PAGE_LEFT,
      PAGE_ACTION_W=96, PAGE_OK_X=PAGE_RIGHT-PAGE_ACTION_W, PAGE_BACK_X=PAGE_OK_X-PAGE_ACTION_W-6};
static const uint16_t *images[7];
static unsigned heights[7];
enum {RULER_PORTRAIT_SIZE=72, RULER_PORTRAIT_COUNT=33};
static const uint16_t *ruler_portraits;
static const uint16_t *strategy_world;
static const uint16_t *strategy_city;
static const uint16_t *strategy_banners;
static const uint16_t *credits_art;
static unsigned strategy_city_frames;
static unsigned read32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int h1_ui_use_buffer(void *p,unsigned n){
    const unsigned char *b=p;
    if(!b||n<16||n>H1R_MAX_BYTES||read32(b)!=0x47533148)return 0;
    unsigned count=read32(b+8);
    unsigned version=read32(b+4);
    int with_credits=version==0x00374955;
    int integrated=version==0x00364955||with_credits;
    if(!((version==0x00324955&&count==7)||(version==0x00334955&&count==8)||
         ((version==0x00344955||version==0x00354955||version==0x00364955)&&count==10)||
         (with_credits&&count==11)))return 0;
    unsigned city_frames=version==0x00354955?3:1;
    unsigned crc=0xffffffff;for(unsigned i=0;i<n-4;i++){crc^=b[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}
    if((crc^0xffffffff)!=read32(b+n-4))return 0;
    const uint16_t *pending[11]={0};
    static const uint16_t widths[]={480,480,480,480,480,480,480,RULER_PORTRAIT_SIZE,480,STRATEGY_CITY_SIZE,480};
    static const uint16_t sizes[]={272,272,272,272,272,272,56,RULER_PORTRAIT_SIZE*RULER_PORTRAIT_COUNT,
                                  STRATEGY_WORLD_HEIGHT,STRATEGY_CITY_SIZE,272};
    unsigned off=12;
    for(unsigned i=0;i<count;i++){
        if(off+12>n-4)return 0;
        unsigned width=read32(b+off),height=read32(b+off+4),len=read32(b+off+8);
        off+=12;
        unsigned expected_width=i==9&&integrated?STRATEGY_BANNER_W:widths[i];
        unsigned expected_height=i==9?(integrated?2*STRATEGY_BANNER_H:STRATEGY_CITY_SIZE*city_frames):sizes[i];
        if(width!=expected_width||height!=expected_height||len!=width*height*(i==9?3:2)||len>n-4-off)return 0;
        pending[i]=(const uint16_t *)(b+off);
        off+=len;
    }
    if(off!=n-4)return 0;
    for(int i=0;i<7;i++){images[i]=pending[i];heights[i]=sizes[i];}
    ruler_portraits=pending[7];
    strategy_world=pending[8];
    strategy_city=integrated?0:pending[9];
    strategy_banners=integrated?pending[9]:0;
    credits_art=pending[10];
    strategy_city_frames=strategy_city?city_frames:0;
    return 1;
}
int h1_ui_assets_load(void){
    if(art)return 1;void *f=bda_sango_fs_fopen("sango.h1r","rb");if(!f)return 0;
    h1_fseek(f,0,2);int n=h1_ftell(f);h1_fseek(f,0,0);
    if(n<=0||n>H1R_MAX_BYTES){h1_fclose(f);return 0;}void *p=h1_alloc(n);
    int ok=p&&h1_fread(p,1,n,f)==(unsigned)n&&h1_ui_use_buffer(p,n);h1_fclose(f);
    if(!ok){if(p)h1_free(p);return 0;}art=p;return 1;
}
void h1_ui_assets_free(void){if(art)h1_free(art);art=0;ruler_portraits=0;strategy_world=0;strategy_city=0;strategy_banners=0;credits_art=0;strategy_city_frames=0;if(strategy_terrain)h1_free(strategy_terrain);strategy_terrain=0;for(int i=0;i<7;i++)images[i]=0;}
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
enum {PLATE_PAPER, PLATE_WOOD, PLATE_SELECTED, PLATE_DARK_INK=0x3102, PLATE_LIGHT_INK=0xff97};
typedef struct { uint16_t edge, rim, gold, top, bottom; } PlateColors;

static void plate(int x,int y,int w,int h,int style)
{
    static const PlateColors colors[]={
        {0x6204,0xb3e8,0xfef4,0xee94,0xe653},
        {0x6204,0xb3e8,0xfef4,0xdd6e,0xc4ab},
        {0x38c2,0xab88,0xeded,0x71a4,0x5143}
    };
    const PlateColors *c=colors+style;
    box(x+1,y,w-2,h,c->edge);
    box(x,y+1,w,h-2,c->edge);
    frame(x+1,y+1,w-2,h-2,c->rim);
    box(x+2,y+2,w-4,h-4,c->bottom);
    box(x+2,y+2,w-4,(h-4)/2,c->top);
    box(x+4,y+2,w-8,1,c->gold);
    box(x+4,y+h-3,w-8,1,c->rim);
    for(int end=0;end<2;end++){
        int side=end?x+w-4:x+2;
        box(side,y+3,2,3,c->rim);
        box(side,y+h-6,2,3,c->rim);
        box(side+(end?0:1),y+3,1,1,c->gold);
        box(side+(end?0:1),y+h-4,1,1,c->gold);
    }
}

static unsigned plate_ink(int selected){return selected?PLATE_LIGHT_INK:PLATE_DARK_INK;}
static void button(int x,int y,int w,int h,const char *text,int selected)
{
    plate(x,y,w,h,selected?PLATE_SELECTED:PLATE_WOOD);
    center(x,y+(h-14)/2,w,text,14,plate_ink(selected));
}
static int hit(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
static int next(int *x,int *y){int e[3];for(;;){sango_input_next_event(e);if(e[0]==5)return e[1];if(e[0]==SANGO_TOUCH_TAP){*x=sango_touch_x(e);*y=sango_touch_y(e);return -1;}if(e[0]==SANGO_TOUCH_PAN_END){int k=sango_touch_swipe_key(e);if(k)return k;}}}
static void foot(const char *p){center(0,247,480,p,12,0xef57);}

typedef struct { unsigned first, selected, contact; int drag; } ListScroll;
typedef struct { int x,y,w,row_height; unsigned visible; } ListRows;

static void list_follow_selection(ListScroll *list,unsigned visible)
{
    if(list->selected<list->first)list->first=list->selected;
    if(list->selected>=list->first+visible)list->first=list->selected-visible+1;
}

static int list_input(ListScroll *list, ListRows rows, unsigned count, int *x, int *y)
{
    int event[3];
    for(;;){
        h1_list_touch_active=1;
        sango_input_next_event(event);
        h1_list_touch_active=0;
        if(event[0]==5)return event[1];
        if(event[0]==SANGO_TOUCH_TAP||event[0]==SANGO_TOUCH_HOLD){*x=sango_touch_x(event);*y=sango_touch_y(event);return -1;}
        if(event[0]!=SANGO_TOUCH_PAN&&event[0]!=SANGO_TOUCH_PAN_END)continue;
        if(!sango_touch_started_in_rect(rows.x,rows.y,rows.w,rows.row_height*rows.visible))continue;
        unsigned contact=sango_touch_contact_sequence();
        if(contact!=list->contact){list->contact=contact;list->drag=0;}
        int drag=list->drag-(int16_t)((unsigned)event[1]>>16);
        int step=drag/rows.row_height;
        if(event[0]==SANGO_TOUCH_PAN_END){
            /* A short flick can advance a row, even between two samples. */
            int tail=drag%rows.row_height;
            step+=(tail>=rows.row_height/2)-(tail<=-rows.row_height/2);
            list->drag=0;
        }else list->drag=drag%rows.row_height;
        int overflow=count>rows.visible;
        unsigned *position=overflow?&list->first:&list->selected;
        int maximum=overflow?count-rows.visible:count-1;
        int offset=(int)*position+step;
        if(offset<=0){offset=0;if(list->drag<0)list->drag=0;}
        if(offset>=maximum){offset=maximum;if(list->drag>0)list->drag=0;}
        if((unsigned)offset==*position)continue;
        *position=offset;
        if(list->selected<list->first)list->selected=list->first;
        if(list->selected>=list->first+rows.visible)list->selected=list->first+rows.visible-1;
        return -2;
    }
}

static void list_scrollbar(int x,int y,int h,unsigned first,unsigned visible,unsigned count)
{
    if(count<=visible)return;
    int thumb=h*visible/count;
    if(thumb<8)thumb=8;
    box(x,y,4,h,0xc4ab);
    box(x,y+(h-thumb)*first/(count-visible),4,thumb,0x71a4);
}

typedef struct { uint8_t x, y; } MapPoint;

enum {
    MAP_WATER = 0x85b5, MAP_LAND = 0xe673, MAP_COAST = 0x8c4d,
    MAP_RIVER = 0x54b2, MAP_RIDGE = 0xa4ed, MAP_SLOPE = 0xbcf0,
    MAP_INK = 0x3186, MAP_GOLD = 0xff06, MAP_OTHER = 0xa925,
    MAP_VACANT = 0x7bce, MAP_LIGHT = 0xff9b
};

/* Schematic terrain in tenths of a strategy cell. City positions come from the game grid. */
static const MapPoint map_coast[] = {
    {0,0}, {120,0}, {120,22}, {115,26}, {110,23}, {108,19}, {103,24},
    {104,29}, {101,34}, {106,40}, {110,49}, {109,63}, {105,69},
    {99,73}, {88,78}, {74,81}, {69,77}, {64,80}, {62,77}, {57,79},
    {48,81}, {44,84}, {41,79}, {29,80}, {22,78}, {23,90}, {0,90}
};
static const MapPoint map_yellow_river[] = {
    {37,29}, {44,27}, {45,24}, {42,19}, {43,14}, {49,12}, {55,14},
    {58,19}, {56,25}, {55,33}, {60,37}, {73,38}, {83,36}, {93,33}, {101,34}
};
static const MapPoint map_yangtze[] = {
    {42,61}, {48,57}, {55,54}, {63,55}, {69,53}, {80,55}, {87,50},
    {96,46}, {102,47}, {110,54}
};
static const MapPoint map_ridges[] = {
    {5,9}, {9,17}, {15,13}, {24,9}, {29,14}, {39,8}, {48,7}, {83,8},
    {94,13}, {112,14}, {68,23}, {75,20}, {8,34}, {19,31}, {12,45},
    {22,44}, {32,40}, {42,47}, {47,44}, {55,44}, {65,33}, {89,30},
    {46,53}, {64,50}, {78,48}, {92,43}, {99,53}, {7,66}, {16,64},
    {23,69}, {30,61}, {27,74}, {57,64}, {64,68}, {89,70}, {10,80}, {16,86}
};

static MapPoint map_pixels(MapPoint point, int cell)
{
    return (MapPoint){point.x*cell/10, point.y*cell/10};
}

static void map_line(int x, int y, MapPoint a, MapPoint b, unsigned color)
{
    int dx=b.x>=a.x?b.x-a.x:a.x-b.x, sx=a.x<b.x?1:-1;
    int dy=b.y>=a.y?a.y-b.y:b.y-a.y, sy=a.y<b.y?1:-1;
    int px=a.x, py=a.y, error=dx+dy;
    for(;;){
        h1_native_pixels[(y+py)*480+x+px]=color;
        if(px==b.x&&py==b.y)break;
        int step=2*error;
        if(step>=dy){error+=dy;px+=sx;}
        if(step<=dx){error+=dx;py+=sy;}
    }
}

static void map_river(int x, int y, int cell, const MapPoint *points, unsigned count)
{
    for(unsigned i=1;i<count;i++)
        map_line(x,y,map_pixels(points[i-1],cell),map_pixels(points[i],cell),MAP_RIVER);
}

static void map_land(int x, int y, int cell)
{
    enum {COAST_POINTS=sizeof(map_coast)/sizeof(map_coast[0])};
    MapPoint coast[COAST_POINTS];
    for(unsigned i=0;i<COAST_POINTS;i++)coast[i]=map_pixels(map_coast[i],cell);
    for(int row=0;row<STRATEGY_ROWS*cell;row++){
        int intersections[COAST_POINTS], count=0;
        for(unsigned i=0;i<COAST_POINTS;i++){
            MapPoint a=coast[i], b=coast[(i+1)%COAST_POINTS];
            if((row<a.y)==(row<b.y))continue;
            int column=a.x+(row-a.y)*(b.x-a.x)/(b.y-a.y), j=count++;
            while(j>0&&intersections[j-1]>column){intersections[j]=intersections[j-1];j--;}
            intersections[j]=column;
        }
        for(int i=1;i<count;i+=2)
            box(x+intersections[i-1],y+row,intersections[i]-intersections[i-1],1,MAP_LAND);
    }
    /* Pull the shoreline endpoints onto the frame after filling the land. */
    coast[2].x--;
    coast[25].y--;
    for(unsigned i=3;i<=25;i++)map_line(x,y,coast[i-1],coast[i],MAP_COAST);
}

static void map_city_marker(int x, int y, unsigned owner, unsigned selected_owner)
{
    if(owner&&owner==selected_owner){
        for(int dy=-4;dy<=4;dy++){
            int radius=dy<0?4+dy:4-dy;
            box(x-radius,y+dy,2*radius+1,1,MAP_INK);
            if(radius>0)box(x-radius+1,y+dy,2*radius-1,1,MAP_GOLD);
        }
    }else{
        box(x-3,y-3,7,7,MAP_INK);
        box(x-2,y-2,5,5,MAP_LIGHT);
        box(x-1,y-1,3,3,owner?MAP_OTHER:MAP_VACANT);
    }
}

static const uint8_t *strategy_city_grid(void)
{
    return (const uint8_t *)sango_resource_item(2,14,g_resource_package_address);
}

static void draw_faction_map(int x, int y, int cell, unsigned selected_owner)
{
    int w=STRATEGY_COLS*cell, h=STRATEGY_ROWS*cell;
    box(x,y,w,h,MAP_WATER);
    map_land(x,y,cell);
    for(unsigned i=0;i<sizeof(map_ridges)/sizeof(map_ridges[0]);i++){
        MapPoint p=map_pixels(map_ridges[i],cell);
        int span=cell/5;
        map_line(x,y,(MapPoint){p.x-span,p.y+1},(MapPoint){p.x,p.y-span},MAP_RIDGE);
        map_line(x,y,(MapPoint){p.x,p.y-span},(MapPoint){p.x+span,p.y+1},MAP_SLOPE);
    }
    map_river(x,y,cell,map_yellow_river,sizeof(map_yellow_river)/sizeof(map_yellow_river[0]));
    map_river(x,y,cell,map_yangtze,sizeof(map_yangtze)/sizeof(map_yangtze[0]));
    frame(x,y,w,h,MAP_COAST);
    const uint8_t *cities=strategy_city_grid();
    for(int row=0;row<STRATEGY_ROWS;row++)for(int col=0;col<STRATEGY_COLS;col++){
        unsigned city=cities[row*STRATEGY_COLS+col];
        if(city){
            unsigned owner=(&g_city_owner_one_based_view)[city*36];
            map_city_marker(x+col*cell+cell/2,y+row*cell+cell/2,owner,selected_owner);
        }
    }
}

static void faction_map_legend(int x, int y, const char *selected)
{
    map_city_marker(x+4,y+6,1,1);
    h1_native_text(x+12,y,selected,12,0x2983);
    map_city_marker(x+43,y+6,2,1);
    h1_native_text(x+50,y,"其他",12,0x2983);
    map_city_marker(x+81,y+6,0,1);
    h1_native_text(x+88,y,"空城",12,0x2983);
}
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

enum {PERIOD_Y=51, PERIOD_H=178, PERIOD_GAP=8,
      PERIOD_W=(PAGE_WIDTH-3*PERIOD_GAP)/4, PERIOD_STEP=PERIOD_W+PERIOD_GAP};

static void draw_period_cards(int selected)
{
    static const char *const titles[]={"董卓弄权","曹操崛起","赤壁之战","三国鼎立"};
    static const char *const years[]={"公元190年","公元198年","公元208年","公元225年"};
    enum {ART_Y=56, ART_H=87, ART_W=PERIOD_W-8};
    page("选择历史时期");
    h1_screen_id=2;
    for(int i=0;i<4;i++){
        int x=PAGE_LEFT+i*PERIOD_STEP;
        int active=i==selected;
        plate(x,PERIOD_Y,PERIOD_W,PERIOD_H,active?PLATE_SELECTED:PLATE_PAPER);
        if(images[5]){
            /* The period art occupies the same 100x87 source slots in H1R v2 and v3. */
            int source_x=16+i*116+(100-ART_W)/2;
            for(int row=ART_Y;row<ART_Y+ART_H;row++)
                memcpy(h1_native_pixels+row*480+x+4,images[5]+row*480+source_x,ART_W*sizeof(uint16_t));
        }
        center(x,157,PERIOD_W,titles[i],16,plate_ink(active));
        box(x+20,184,PERIOD_W-40,1,active?0xeded:0xb3e8);
        center(x,198,PERIOD_W,years[i],12,plate_ink(active));
    }
    foot("左右选择 · 确定进入 · 返回主菜单");
    h1_native_flip();
}

uint32_t sango_menu_select_animated(uint16_t background,short base){
    (void)base;int selected=0,x=0,y=0;h1_layout_game=0;strategy_y=0;
    for(;;){
        if(background==44)h1_draw_main(selected);
        else draw_period_cards(selected);
        int key=next(&x,&y);if(key==1)return 255;if(key==0x1c)return selected;
        if(key==-1){if(background==44){for(int i=0;i<4;i++)if(hit(x,y,247,68+41*i,212,38))return i;}
            else for(int i=0;i<4;i++)if(hit(x,y,PAGE_LEFT+i*PERIOD_STEP,PERIOD_Y,PERIOD_W,PERIOD_H))return i;}
        else if(key==0x67||key==0x69)selected=(selected+3)&3;else if(key==0x6c||key==0x6a)selected=(selected+1)&3;
    }
}
uint32_t sango_menu_choose_ruler(uint32_t count){
    if(!count)return 255;ListScroll list={0};int x=0,y=0;char name[32];
    enum {LIST_W=114, ROW_X=PAGE_LEFT+6, ROW_W=LIST_W-17, AVATAR_X=PAGE_RIGHT-78};
    ListRows rows={ROW_X,78,LIST_W-8,24,count<6?count:6};
    for(;;){
        list_follow_selection(&list,rows.visible);
        page("选择君主");h1_screen_id=3;plate(PAGE_LEFT,48,LIST_W,181,PLATE_PAPER);
        center(PAGE_LEFT,52,LIST_W,"天下群雄",14,0x2983);
        for(unsigned row=0;row<rows.visible;row++){unsigned i=list.first+row;int yy=78+row*24;sango_load_named_text(g_temp_character_ids_address[i],name);button(ROW_X,yy,ROW_W,22,name,i==list.selected);}
        list_scrollbar(PAGE_LEFT+LIST_W-8,79,140,list.first,rows.visible,count);
        unsigned who=g_temp_character_ids_address[list.selected],scenario=g_scenario_id_view2;
        txt(162,49,"势力分布");
        faction_map_legend(231,50,"所选");
        draw_faction_map(162,69,15,who+1);
        if(!ruler_portrait(scenario,who,AVATAR_X,66,72)){
            if(scenario>=1&&scenario<=4&&who<19&&ruler_portrait_frames[scenario-1][who]!=255)
                sprite(53,ruler_portrait_frames[scenario-1][who],AVATAR_X,66,72,72);
            else sprite(scenario+47,who,AVATAR_X,66,72,72);
        }
        sango_load_named_text(who,name);center(PAGE_OK_X+6,148,PAGE_ACTION_W, name,24,0x2983);
        uint8_t cities[40];unsigned cities_count=sango_faction_list_cities(who,(int)cities);
        txt(354,181,"城池");number(410,181,cities_count,0x2983);
        button(PAGE_BACK_X,212,PAGE_ACTION_W,27,"返回",0);button(PAGE_OK_X,212,PAGE_ACTION_W,27,"确定",1);foot("拖动翻动名单 · 点击名字预览");h1_native_flip();
        int key=list_input(&list,rows,count,&x,&y);if(key==1||(key==-1&&hit(x,y,PAGE_BACK_X,212,PAGE_ACTION_W,27)))return 255;
        if(key==0x1c||(key==-1&&hit(x,y,PAGE_OK_X,212,PAGE_ACTION_W,27)))return who;
        if(key==-1&&hit(x,y,ROW_X,78,ROW_W,rows.visible*24)&&(y-78)%24<22)list.selected=list.first+(y-78)/24;
        if(key==0x67&&list.selected)list.selected--;if(key==0x6c&&list.selected+1<count)list.selected++;
        if(key==0x69)list.selected=list.selected>=6?list.selected-6:0;if(key==0x6a)list.selected=list.selected+6<count?list.selected+6:count-1;
    }
}
void sango_menu_show_credits(void)
{
    Saved saved=save_screen();
    h1_ui_assets_load();
    if(credits_art){
        h1_native_active=1;
        memcpy(h1_native_pixels,credits_art,sizeof(h1_native_pixels));
    }else{
        page("制作群组");
        txt(PAGE_LEFT,58,"策划  BBK gamegroup");
        txt(PAGE_LEFT,88,"程序  通宵虫、南方小鬼");
        txt(PAGE_LEFT,118,"美工  Sunday");
        txt(PAGE_LEFT,158,"H1 移植  HelloClyde");
        foot("点击或按键返回");
    }
    h1_screen_id=5;
    h1_native_flip();
    int x,y;
    next(&x,&y);
    restore(saved);
}
void sango_settings_show(void){
    Saved saved=save_screen();int selected=0,x=0,y=0;
    enum {SETTINGS_ACTION_W=(PAGE_WIDTH-20)/2, SETTINGS_OK_X=PAGE_RIGHT-SETTINGS_ACTION_W};
    for(;;){page("游戏设置");h1_screen_id=4;txt(PAGE_LEFT,65,"背景音乐");button(260,56,170,34,sango_music_is_enabled()?"开启":"关闭",selected==0);
        txt(PAGE_LEFT,118,"音乐音量");button(200,106,48,34,"－",selected==1);button(382,106,48,34,"＋",selected==1);number(291,115,sango_music_get_volume()*10,0x2983);txt(327,115,"%");
        button(PAGE_LEFT,164,SETTINGS_ACTION_W,42,"制作群组",selected==2);button(SETTINGS_OK_X,164,SETTINGS_ACTION_W,42,"保存并返回",selected==3);foot("方向键调整 · 点击按钮操作");h1_native_flip();
        int key=next(&x,&y),action=-1;if(key==1)action=3;
        if(key==0x67)selected=(selected+3)&3;if(key==0x6c)selected=(selected+1)&3;
        if(key==0x1c)action=selected;if(key==0x69&&selected==1&&sango_music_get_volume())sango_music_set_volume(sango_music_get_volume()-1);
        if(key==0x6a&&selected==1&&sango_music_get_volume()<10)sango_music_set_volume(sango_music_get_volume()+1);
        if(key==-1){if(hit(x,y,260,56,170,34))action=0;if(hit(x,y,200,106,48,34)&&sango_music_get_volume())sango_music_set_volume(sango_music_get_volume()-1);if(hit(x,y,382,106,48,34)&&sango_music_get_volume()<10)sango_music_set_volume(sango_music_get_volume()+1);if(hit(x,y,PAGE_LEFT,164,SETTINGS_ACTION_W,42))action=2;if(hit(x,y,SETTINGS_OK_X,164,SETTINGS_ACTION_W,42))action=3;}
        if(action==0)sango_music_set_enabled(!sango_music_is_enabled());if(action==1)sango_music_set_volume((sango_music_get_volume()+1)%11);
        if(action==2)sango_menu_show_credits();
        if(action==3){uint8_t b[8]={'S','G','S',1,0,0,0,0};b[4]=sango_music_is_enabled();b[5]=sango_music_get_volume();b[7]=0xa5^b[4]^b[5];sango_settings_sdk_write(b,8);restore(saved);return;}
    }
}
static Saved *command_background;
static unsigned command_left;
static unsigned menu(char labels[][80],unsigned count,unsigned selected,const Saved *saved,int ax,int ay,int keep_selection){
    int x=0,y=0,w=76;unsigned visible=count<7?count:7;if(!count||!saved->pixels)return 255;
    ListScroll list={.selected=selected};
    int commit=0;
    for(unsigned i=0;i<count;i++){int n=length(labels[i]);while(n&&labels[i][n-1]==' ')labels[i][--n]=0;if(n*7+24>w)w=n*7+24;}
    if(count>visible)w+=8;if(w>228)w=228;
    int h=visible*26+12;if(ax<8)ax=8;if(ax>472-w)ax=472-w;if(ay<7)ay=7;if(ay>232-h)ay=232-h;
    ListRows rows={ax+5,ay+6,w-10,26,visible};
    for(;;){list_follow_selection(&list,visible);
        memcpy(h1_native_pixels,saved->pixels,sizeof(h1_native_pixels));h1_native_active=1;h1_screen_id=20;
        box(ax+3,ay+3,w,h,0x3020);plate(ax,ay,w,h,PLATE_PAPER);
        for(unsigned row=0;row<visible;row++){
            unsigned i=list.first+row;int yy=ay+6+row*26;int active=i==list.selected;
            if(active)plate(ax+5,yy,w-10-(count>visible?8:0),26,PLATE_SELECTED);
            char clipped[80];unsigned n=0;int pixels=0,limit=w-24-(count>visible?8:0);const unsigned char *p=(unsigned char *)labels[i];
            while(*p){unsigned bytes=*p>=128&&p[1]?2:1;int advance=bytes==2?14:7;if(pixels+advance>limit)break;pixels+=advance;clipped[n++]=*p++;if(bytes==2)clipped[n++]=*p++;}clipped[n]=0;h1_native_text(ax+12,yy+6,clipped,14,plate_ink(active));
        }
        list_scrollbar(ax+w-9,ay+8,visible*26-4,list.first,visible,count);
        if(commit)return list.selected; /* The child takes this frame as its background. */
        h1_native_flip();int key=list_input(&list,rows,count,&x,&y);
        if(key==1)return 255;if(key==0x1c)return list.selected;
        if(key==0x67)list.selected=(list.selected+count-1)%count;if(key==0x6c)list.selected=(list.selected+1)%count;
        if(key==-1){
            if(!hit(x,y,ax,ay,w,h))return 255;
            if(hit(x,y,ax+5,ay+6,w-10-(count>visible?8:0),visible*26)){
                unsigned tapped=list.first+(y-ay-6)/26;
                if(!keep_selection||tapped==list.selected)return tapped;
                list.selected=tapped;
                commit=1;
            }
        }
    }
}
uint32_t sango_ui_choose_option(uint16_t *b,uint32_t initial,char *text){
    if(command_background&&b[0]<command_left)command_left=b[0];
    int parent_menu=command_background&&b[0]==command_left;
    if(parent_menu)memcpy(h1_native_pixels,command_background->pixels,sizeof(h1_native_pixels));
    Saved saved=save_screen();if(!saved.pixels)return 255;char rows[40][80];unsigned stride=(b[2]-b[0])/6;if(!stride)stride=1;unsigned count=length(text)/stride;if(count>40)count=40;
    for(unsigned i=0;i<count;i++){unsigned n=stride<79?stride:79;memcpy(rows[i],text+i*stride,n);rows[i][n]=0;}
    unsigned choice=menu(rows,count,initial<count?initial:0,&saved,b[0]*2-6,(int)b[1]-3,parent_menu);
    if(command_background&&choice!=255)h1_free(saved.pixels);else restore(saved);return choice;
}
uint32_t sango_city_choose_command_legacy(void);
uint32_t sango_city_choose_command(void){
    Saved saved=save_screen();if(!saved.pixels)return 255;Saved *previous=command_background;unsigned previous_left=command_left;
    command_background=&saved;command_left=65535;unsigned choice=sango_city_choose_command_legacy();
    command_background=previous;command_left=previous_left;restore(saved);return choice;
}
uint32_t sango_ui_select_character(int ids,uint32_t count,uint32_t l,uint32_t t,uint8_t r,uint8_t b){
    (void)l;(void)t;(void)r;(void)b;if(!count)return 255;Saved saved=save_screen();ListScroll list={0};int x,y;char name[32];
    enum {VISIBLE=6, ACTION_Y=213, ACTION_H=26};
    ListRows rows={PAGE_LEFT,71,PAGE_WIDTH,23,count<VISIBLE?count:VISIBLE};
    for(;;){list_follow_selection(&list,rows.visible);page("选择武将");h1_screen_id=7;
        txt(PAGE_LEFT+8,48,"姓名");txt(134,48,"归属");txt(220,48,"等级");txt(278,48,"武力");txt(336,48,"智力");txt(394,48,"兵力");
        for(unsigned row=0;row<rows.visible;row++){
            unsigned i=list.first+row,who=((uint8_t *)ids)[i];int yy=73+row*23;SangoCharacter *c=(SangoCharacter *)(g_character_table_address+16*who);
            int active=i==list.selected;unsigned ink=plate_ink(active);
            if(active)plate(PAGE_LEFT,yy-2,PAGE_WIDTH-7,23,PLATE_SELECTED);
            sango_load_named_text(who,name);h1_native_text(PAGE_LEFT+8,yy,name,14,ink);
            sango_character_format_attribute(who,0,(uint8_t *)name);h1_native_text(134,yy,name,14,ink);
            number(228,yy,c->level,ink);number(286,yy,c->strength,ink);number(344,yy,c->intelligence,ink);number(394,yy,c->troops,ink);
        }
        list_scrollbar(PAGE_RIGHT-5,72,rows.visible*23-2,list.first,rows.visible,count);
        button(PAGE_BACK_X,ACTION_Y,PAGE_ACTION_W,ACTION_H,"返回",0);
        button(PAGE_OK_X,ACTION_Y,PAGE_ACTION_W,ACTION_H,"确定",1);
        foot("拖动翻动名单 · 点选后确定");h1_native_flip();
        int key=list_input(&list,rows,count,&x,&y);
        if(key==1||(key==-1&&hit(x,y,PAGE_BACK_X,ACTION_Y,PAGE_ACTION_W,ACTION_H))){restore(saved);return 255;}
        if(key==0x1c||(key==-1&&hit(x,y,PAGE_OK_X,ACTION_Y,PAGE_ACTION_W,ACTION_H))){restore(saved);return list.selected;}
        if(key==0x67)list.selected=(list.selected+count-1)%count;if(key==0x6c)list.selected=(list.selected+1)%count;
        if(key==-1&&hit(x,y,rows.x,rows.y,rows.w,rows.visible*rows.row_height))
            list.selected=list.first+(y-rows.y)/rows.row_height;
    }
}
uint32_t sango_ui_select_item(int ids,uint32_t count,uint32_t l,uint32_t t,uint8_t r,uint8_t b){(void)r;(void)b;Saved saved=save_screen();if(!saved.pixels)return 255;char rows[40][80];if(count>40)count=40;for(unsigned i=0;i<count;i++)sango_item_read_name(((uint8_t *)ids)[i],rows[i]);unsigned result=menu(rows,count,0,&saved,l*2-6,(int)t-3,0);restore(saved);return result;}
uint32_t sango_ui_select_number(uint32_t initial,uint32_t maximum){
    Saved saved=save_screen();unsigned value=initial>maximum?maximum:initial,step=1;if(maximum>9999)maximum=9999;if(value>maximum)value=maximum;int x,y;
    for(;;){page("输入数量");h1_screen_id=8;txt(44,68,"上限");number(108,68,maximum,0x2983);txt(44,102,"每次调整");number(140,102,step,0x2983);
        button(60,148,72,42,"－",0);plate(150,135,180,62,PLATE_PAPER);char s[16];sango_format_integer(value,(uint8_t *)s,10);center(150,151,180,s,24,PLATE_DARK_INK);button(348,148,72,42,"＋",0);
        button(PAGE_BACK_X,211,PAGE_ACTION_W,28,"返回",0);button(PAGE_OK_X,211,PAGE_ACTION_W,28,"确定",1);foot("左右选步长 · 上下改数量 · 点击加减");h1_native_flip();int k=next(&x,&y);
        if(k==1||(k==-1&&hit(x,y,PAGE_BACK_X,211,PAGE_ACTION_W,28))){restore(saved);return 65535;}if(k==0x1c||(k==-1&&hit(x,y,PAGE_OK_X,211,PAGE_ACTION_W,28))){restore(saved);return value;}
        if(k==0x69&&step<1000)step*=10;if(k==0x6a&&step>1)step/=10;
        if(k==0x67||(k==-1&&hit(x,y,348,148,72,42)))value=value+step>maximum?maximum:value+step;
        if(k==0x6c||(k==-1&&hit(x,y,60,148,72,42)))value=value<step?0:value-step;
    }
}
static void paragraph(int x,int y,int width,const char *p){char line[80];while(*p&&y<228){unsigned n=0;while(*p&&n<(unsigned)(width/8)&&n<76){unsigned bytes=(uint8_t)*p>=128?2:1;if(n+bytes>(unsigned)(width/8))break;line[n++]=*p++;if(bytes==2&&*p)line[n++]=*p++;}line[n]=0;txt(x,y,line);y+=22;}}

enum {NOTE_WIDTH=370, NOTE_PAD=14, NOTE_ADVANCE=7, NOTE_LINE_HEIGHT=19, NOTE_ROWS=7,
      NOTE_COLUMNS=(NOTE_WIDTH-2*NOTE_PAD)/NOTE_ADVANCE, NOTE_PORTRAIT=44, NOTE_PORTRAIT_GAP=12};

static int read_note_line(const char **cursor,char *line,int columns)
{
    const char *p=*cursor;
    int used=0;
    /* The native 14px renderer advances 7px per GBK byte. Copy both bytes
       of a Chinese glyph together so a wrap never splits it. */
    while(*p&&*p!='\r'&&*p!='\n'){
        int bytes=(uint8_t)*p>=0x81&&(uint8_t)p[1]>=0x40?2:1;
        if(used+bytes>columns)break;
        while(bytes--)line[used++]=*p++;
    }
    if(*p=='\r')p++;
    if(*p=='\n')p++;
    line[used]=0;
    *cursor=p;
    return used*NOTE_ADVANCE;
}

static void show_strategy_note(const char *text,const char *name,int who,int ticks)
{
    Saved saved=save_screen();
    int portrait_width=name?NOTE_PORTRAIT+NOTE_PORTRAIT_GAP:0;
    int heading_height=name?20:0;
    int columns=(NOTE_WIDTH-2*NOTE_PAD-portrait_width)/NOTE_ADVANCE;
    h1_screen_id=12;
    do{
        char lines[NOTE_ROWS][NOTE_COLUMNS+1];
        int rows=0, text_width=name?length(name)*NOTE_ADVANCE:0;
        do{
            int width=read_note_line(&text,lines[rows++],columns);
            if(width>text_width)text_width=width;
        }while(*text&&rows<NOTE_ROWS);

        int more=*text&&ticks&&saved.pixels;
        int width=2*NOTE_PAD+portrait_width+text_width;
        int height=22+heading_height+rows*NOTE_LINE_HEIGHT+(more?14:0);
        if(width<108)width=108;
        if(name&&height<NOTE_PORTRAIT+22)height=NOTE_PORTRAIT+22;
        int x=(480-width)/2, y=(STRATEGY_HEIGHT-height)/2;
        int left=x+NOTE_PAD+portrait_width, top=y+10;
        if(saved.pixels)memcpy(h1_native_pixels,saved.pixels,sizeof(h1_native_pixels));
        box(x+3,y+3,width,height,0x3020);
        plate(x,y,width,height,PLATE_PAPER);
        if(name){
            int px=x+NOTE_PAD, py=y+11;
            if(!ruler_portrait(g_scenario_id_view2,who,px,py,NOTE_PORTRAIT))
                sprite(g_scenario_id_view2+47,who,px,py,NOTE_PORTRAIT,NOTE_PORTRAIT);
            frame(px-1,py-1,NOTE_PORTRAIT+2,NOTE_PORTRAIT+2,0x8a45);
            h1_native_text(left,top,name,14,0x79c4);
        }
        if(!name&&rows==1)center(x,top,width,lines[0],14,0x2983);
        else for(int row=0;row<rows;row++)txt(left,top+heading_height+row*NOTE_LINE_HEIGHT,lines[row]);
        if(more)h1_native_text(x+width-NOTE_PAD-36,y+height-17,"下一页",12,0x79c4);
        h1_native_flip();
        if(!ticks||sango_input_wait(ticks,1)==1||!more)break;
    }while(*text);
    if(ticks)restore(saved);
    else if(saved.pixels)h1_free(saved.pixels);
}

void sango_ui_show_message(char *p,uint32_t wait)
{
    if(h1_layout_game==1&&h1_native_active){show_strategy_note(p,0,0,(wait&255)*40);return;}
    Saved saved=save_screen();
    page("消息");h1_screen_id=12;
    paragraph(PAGE_LEFT,64,PAGE_WIDTH,p);
    foot("点击或按键继续");h1_native_flip();
    if(wait){sango_input_wait((wait&255)*40,1);restore(saved);}
    else if(saved.pixels)h1_free(saved.pixels);
}
void sango_ui_show_character_message(uint8_t who,char *p)
{
    char name[32];sango_load_named_text(who,name);
    if(h1_layout_game==1&&h1_native_active){show_strategy_note(p,name,who,100);return;}
    Saved saved=save_screen();
    page(name);
    if(!ruler_portrait(g_scenario_id_view2,who,PAGE_LEFT,67,72))
        sprite(g_scenario_id_view2+47,who,PAGE_LEFT,67,72,72);
    paragraph(128,64,PAGE_RIGHT-128,p);
    foot("点击或按键继续");h1_native_flip();
    sango_input_wait(100,1);restore(saved);
}

static void show_battle_notice(const char *message,unsigned resource,unsigned image,
                               int art_width,int art_height,short ticks)
{
    Saved saved=save_screen();
    if(saved.pixels){
        enum {PAD=12, FONT=16, CAPTION_HEIGHT=24};
        int center_height=h1_layout_game==2?H1_BATTLE_HEIGHT:STRATEGY_HEIGHT;
        int text_width=message?length(message)*FONT/2:0;
        int width=(text_width>art_width?text_width:art_width)+2*PAD;
        int height=2*PAD+art_height+(message?CAPTION_HEIGHT:0);
        int x=(480-width)/2,y=(center_height-height)/2;
        h1_native_active=1;
        h1_screen_id=12;
        plate(x,y,width,height,PLATE_PAPER);
        if(art_width)sprite(resource,image,(480-art_width)/2,y+PAD,art_width,art_height);
        if(message)center(x,y+PAD+art_height+4,width,message,FONT,PLATE_DARK_INK);
        h1_native_flip();
    }
    sango_input_wait(ticks,0);
    restore(saved);
}

void h1_battle_show_weather(unsigned weather)
{
    show_battle_notice(NULL,weather+0x1b,0,130,64,0x32);
}

void h1_battle_show_day(unsigned day)
{
    char message[32]="第 ",digits[12];
    sango_format_integer(day,digits,10);
    bbk_strcat(message,digits);
    bbk_strcat(message," 天");
    show_battle_notice(message,0,0,0,0,0x32);
}

void h1_show_army_notice(const char *message,unsigned image,short ticks)
{
    show_battle_notice(message,0x4c,image,84,64,ticks);
}

uint32_t sango_ui_show_city_details(uint8_t city){Saved saved=save_screen();char name[80],value[80];sango_city_read_name(city,name);page(name);h1_screen_id=11;for(unsigned i=0;i<11;i++){sango_city_format_attribute(city,i,value,315);txt(PAGE_LEFT+(i%2)*PAGE_WIDTH/2,58+(i/2)*27,value);}foot("确定选择本城 · 返回取消");h1_native_flip();int x,y,k=next(&x,&y);restore(saved);return k==0x1c||k==-1;}
void sango_save_draw_slots(void){}
uint32_t sango_save_choose_slot(char load){
    Saved saved=save_screen();ListScroll list={0};int x,y;char name[32];
    enum {SLOT_Y=54, SLOT_H=46, SLOT_STEP=55, SLOT_ACTION_Y=222, SLOT_ACTION_H=20};
    ListRows rows={PAGE_LEFT,SLOT_Y,PAGE_WIDTH,SLOT_STEP,3};
    for(;;){page(load?"读取进度":"保存进度");h1_screen_id=load?10:9;
        for(unsigned i=0;i<3;i++){
            int yy=SLOT_Y+i*SLOT_STEP,active=i==list.selected;unsigned ink=plate_ink(active);
            plate(PAGE_LEFT,yy,PAGE_WIDTH,SLOT_H,active?PLATE_SELECTED:PLATE_PAPER);
            h1_native_text(PAGE_LEFT+14,yy+14,"档位",14,ink);number(PAGE_LEFT+58,yy+14,i+1,ink);
            char path[]="sango0.sav";path[5]='0'+i*2;
            void *f=bda_sango_fs_fopen(path,"rb");
            if(f){
                uint8_t h[8];unsigned n=h1_fread(f,1,8,h);h1_fclose(f);
                if(n==8){
                    unsigned old=g_scenario_id_view2;g_scenario_id_view2=h[0];sango_load_named_text(h[1],name);g_scenario_id_view2=old;
                    h1_native_text(128,yy+14,name,14,ink);number(262,yy+14,h[2]|h[3]<<8,ink);
                    h1_native_text(298,yy+14,"年",14,ink);number(334,yy+14,h[7],ink);h1_native_text(362,yy+14,"月",14,ink);
                }
            }else h1_native_text(154,yy+14,"空白进度",14,ink);
        }
        button(PAGE_BACK_X,SLOT_ACTION_Y,PAGE_ACTION_W,SLOT_ACTION_H,"返回",0);
        button(PAGE_OK_X,SLOT_ACTION_Y,PAGE_ACTION_W,SLOT_ACTION_H,load?"读取":"保存",1);
        foot("上下滑动选择档位");h1_native_flip();
        int k=list_input(&list,rows,3,&x,&y);
        if(k==1||(k==-1&&hit(x,y,PAGE_BACK_X,SLOT_ACTION_Y,PAGE_ACTION_W,SLOT_ACTION_H))){restore(saved);return 255;}
        int commit=k==0x1c||(k==-1&&hit(x,y,PAGE_OK_X,SLOT_ACTION_Y,PAGE_ACTION_W,SLOT_ACTION_H));
        if(k==-1&&hit(x,y,PAGE_LEFT,SLOT_Y,PAGE_WIDTH,3*SLOT_STEP)&&(y-SLOT_Y)%SLOT_STEP<SLOT_H){list.selected=(y-SLOT_Y)/SLOT_STEP;commit=1;}
        if(k==0x67)list.selected=(list.selected+2)%3;if(k==0x6c)list.selected=(list.selected+1)%3;
        if(commit){unsigned ok=load?sango_save_load_slot(list.selected):sango_save_write_slot(list.selected);if(ok){restore(saved);return list.selected;}}
    }
}
static uint8_t *map_state;
static int clamp_coordinate(int value,int min,int max){
    if(value<min)return min;
    if(value>max)return max;
    return value;
}
void h1_strategy_hud(void){
    box(0,240,480,32,0x3020);char name[32];if(!ruler_portrait(g_scenario_id_view2,g_player_character,4,244,24))sprite(g_scenario_id_view2+47,g_player_character,4,244,24,24);sango_load_named_text(g_player_character,name);h1_native_text(34,249,name,14,0xff16);
    number(108,249,g_current_year,0xff16);h1_native_text(140,249,"年",14,0xff16);number(166,249,g_current_month,0xff16);h1_native_text(187,249,"月",14,0xff16);
    if(map_state){uint8_t *cities=(uint8_t *)sango_resource_item(2,14,g_resource_package_address);unsigned city=cities[map_state[3]*12+map_state[2]];if(city){sango_city_read_name(city-1,name);h1_native_text(218,249,name,14,0xff16);}}
    button(298,243,55,26,"信息",0);button(358,243,55,26,"查找",0);button(418,243,58,26,"菜单",0);
}

static void draw_strategy_city_focus(int x,int y,unsigned city)
{
    enum {TAG_FONT=12, TAG_HEIGHT=15, TAG_Y=29};
    char name[32];
    sango_city_read_name(city-1,name);
    int width=length(name)*TAG_FONT/2+10;
    int left=clamp_coordinate(x+(STRATEGY_TILE-width)/2,1,479-width);
    int top=clamp_coordinate(y+TAG_Y,1,STRATEGY_HEIGHT-TAG_HEIGHT-1);
    plate(left,top,width,TAG_HEIGHT,PLATE_SELECTED);
    center(left,top+1,width,name,TAG_FONT,PLATE_LIGHT_INK);
}

static void draw_strategy_terrain_focus(int x,int y)
{
    int cx=x+STRATEGY_TILE/2, cy=y+STRATEGY_TILE/2;
    for(int dy=-6;dy<=6;dy++){
        int row=cy+dy;
        if((unsigned)row>=STRATEGY_HEIGHT)continue;
        uint16_t *pixels=h1_native_pixels+row*480+cx;
        for(int dx=-18;dx<=18;dx++){
            int radius=36*dx*dx+324*dy*dy;
            if(radius>11700)continue;
            if(radius>=10000)pixels[dx]=h1_blend_rgb565(0x6244,pixels[dx],185);
            else if(radius>=7700)pixels[dx]=h1_blend_rgb565(0xff33,pixels[dx],dy>=0?250:205);
            else if(radius>=6600)pixels[dx]=h1_blend_rgb565(0xda47,pixels[dx],158);
            else pixels[dx]=h1_blend_rgb565(0xf6b1,pixels[dx],36);
        }
    }
}

static void blit_strategy_overlay(const uint16_t *strip,int frames,int frame,int left,int top,int width,int height)
{
    int pixels=width*height, offset=frame*pixels;
    const uint16_t *colors=strip+offset;
    const uint8_t *alpha=(const uint8_t *)(strip+frames*pixels)+offset;
    int first=top<0?-top:0, last=STRATEGY_HEIGHT-top;
    if(last>height)last=height;
    for(int row=first;row<last;row++){
        int src=row*width, dst=(top+row)*480+left;
        for(int col=0;col<width;col++){
            unsigned a=alpha[src+col];
            if(a)h1_native_pixels[dst+col]=a==255?colors[src+col]:
                h1_blend_rgb565(colors[src+col],h1_native_pixels[dst+col],a);
        }
    }
}

static void draw_strategy_city(int x,int y,unsigned owner)
{
    if(strategy_banners){
        if(owner)blit_strategy_overlay(strategy_banners,2,owner==g_player_character+1,
                                       x+27,y+4,STRATEGY_BANNER_W,STRATEGY_BANNER_H);
        return;
    }
    if(!strategy_city){
        sprite(55,owner==g_player_character+1?8:owner?7:0,x+8,y+8,24,24);
        return;
    }
    unsigned faction=owner==g_player_character+1?2:owner?1:0;
    int left=x+(STRATEGY_TILE-STRATEGY_CITY_SIZE)/2, top=y+4;
    blit_strategy_overlay(strategy_city,strategy_city_frames,strategy_city_frames==3?faction:0,
                          left,top,STRATEGY_CITY_SIZE,STRATEGY_CITY_SIZE);
    if(strategy_city_frames==3)return;
    /* A v4 asset has no painted banner and still needs its ownership marker. */
    unsigned flag=faction==2?0xfec4:faction==1?0xc1e4:0x96b9;
    int pole=x+23, pennant=y+4;
    box(pole,pennant,2,13,0x2922);
    box(pole+2,pennant,10,2,0x2922);
    box(pole+2,pennant+2,8,4,0x2922);
    box(pole+2,pennant+6,5,2,0x2922);
    box(pole+2,pennant+1,8,2,flag);
    box(pole+2,pennant+3,6,2,flag);
    box(pole+2,pennant+5,4,2,flag);
    box(pole,pennant-1,2,2,0xff37);
}

uint32_t sango_strategy_draw_map(uint8_t *state,uint8_t scroll){
    (void)scroll;h1_screen_id=13;map_state=state;h1_native_active=1;h1_layout_game=1;state[0]=0;
    if(state[1]!=strategy_y/STRATEGY_TILE)
        strategy_y=clamp_coordinate(state[1]*STRATEGY_TILE,0,STRATEGY_WORLD_HEIGHT-STRATEGY_HEIGHT);
    if(state[3]<strategy_y/STRATEGY_TILE)strategy_y=state[3]*STRATEGY_TILE;
    if(state[3]>(strategy_y+STRATEGY_HEIGHT-1)/STRATEGY_TILE)
        strategy_y=(state[3]+1)*STRATEGY_TILE-STRATEGY_HEIGHT;
    state[1]=strategy_y/STRATEGY_TILE;
    const uint8_t *terrain=0;
    const uint8_t *cities=strategy_city_grid();
    if(!strategy_world&&!strategy_terrain){
        terrain=(const uint8_t *)sango_resource_item(54,1,g_resource_package_address);
        strategy_terrain=h1_alloc(480*STRATEGY_WORLD_HEIGHT*sizeof(*strategy_terrain));
        if(strategy_terrain){
            for(int row=0;row<STRATEGY_ROWS;row++){
                for(int col=0;col<STRATEGY_COLS;col++){
                    unsigned index=row*STRATEGY_COLS+col;
                    blit_sprite(strategy_terrain,STRATEGY_WORLD_HEIGHT,terrain+6+index*256,32,32,0,col*40,row*40,40,40);
                }
            }
        }
    }
    const uint16_t *world=strategy_world?strategy_world:strategy_terrain;
    if(world)memcpy(h1_native_pixels,world+strategy_y*480,480*STRATEGY_HEIGHT*sizeof(*world));
    int last_row=(strategy_y+STRATEGY_HEIGHT-1)/STRATEGY_TILE;
    unsigned selected=state[3]*STRATEGY_COLS+state[2];
    for(int row=state[1];row<=last_row;row++)for(int col=0;col<STRATEGY_COLS;col++){
        int y=row*STRATEGY_TILE-strategy_y;
        unsigned index=row*STRATEGY_COLS+col;
        if(!world)sprite_pixels(terrain+6+index*256,32,32,0,col*40,y,40,40);
        unsigned city=cities[index];
        if(city)draw_strategy_city(col*40,y,(&g_city_owner_one_based_view)[city*36]);
    }
    unsigned city=cities[selected];
    int focus_x=state[2]*STRATEGY_TILE,focus_y=state[3]*STRATEGY_TILE-strategy_y;
    if(city)draw_strategy_city_focus(focus_x,focus_y,city);
    else draw_strategy_terrain_focus(focus_x,focus_y);
    h1_strategy_hud();h1_native_flip();return city;
}
void sango_strategy_present(void){h1_native_active=1;h1_native_flip();}
static void show_faction_overview(void)
{
    Saved saved=save_screen();
    page("天下形势");
    h1_screen_id=14;
    draw_faction_map(120,45,20,g_player_character+1);
    faction_map_legend(184,229,"我方");
    foot("点击或按键返回");
    h1_native_flip();
    int x,y;
    next(&x,&y);
    restore(saved);
}
uint32_t sango_strategy_select_city(uint8_t *s){
    int x=0,y=0;unsigned selected=sango_strategy_draw_map(s,0);
    for(;;){int e[3];sango_input_next_event(e);int k=e[0]==5?e[1]:0;
        if(e[0]==SANGO_TOUCH_TAP){x=sango_touch_x(e);y=sango_touch_y(e);if(y<240){s[2]=x/40;s[3]=(strategy_y+y)/40;selected=sango_strategy_draw_map(s,0);if(selected)return selected-1;continue;}if(x>=418)return 255;if(x>=358){show_faction_overview();continue;}if(x>=298&&selected)sango_ui_show_city_details(selected-1);continue;}
        if(e[0]==SANGO_TOUCH_PAN || e[0]==SANGO_TOUCH_PAN_END){
            if(!sango_touch_started_in_map(480,STRATEGY_HEIGHT))continue;
            int delta=(int16_t)((unsigned)e[1]>>16);
            int next_y=clamp_coordinate(strategy_y-delta,0,STRATEGY_WORLD_HEIGHT-STRATEGY_HEIGHT);
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
    if(g_battle_weather)sprite(9,g_battle_weather-1,298,220,18,18);
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
    for(unsigned row=0;row<10;row++)for(unsigned col=0;col<21;col++){
        unsigned tile=((uint8_t *)g_viewport_tiles_address)[row*21+col];
        if(!h1_battle_tile_art(tile,col*16,row*16,g_game_context))
            sango_raster_draw_opaque(col*16,row*16,16,16,tiles+6+tile*64,g_game_context);
    }
}
void sango_battle_info_show(void){
    Saved saved=save_screen();unsigned unit=sango_battle_find_unit_at(g_battle_cursor_x,g_battle_cursor_y);
    char title[40],value[160];
    if(unit<20){unsigned who=sango_battle_unit_character(unit);SangoCharacter *c=(SangoCharacter *)(g_character_table_address+who*16);SangoBattleUnit *u=(SangoBattleUnit *)(&g_first_unit_x)+unit;
        sango_load_named_text(who,title);page(title);if(!ruler_portrait(g_scenario_id_view2,who,PAGE_LEFT,60,72))sprite(g_scenario_id_view2+47,who,PAGE_LEFT,60,72,72);
        const char *labels[]={"等级","兵力","武力","智力","生命","技能","体力","忠诚"};unsigned values[]={c->level,c->troops,c->strength,c->intelligence,u->life,u->energy,c->stamina,c->loyalty};
        for(int i=0;i<8;i++){int x=142+(i&1)*156,y=62+(i/2)*37;txt(x,y,labels[i]);number(x+70,y,values[i],0x2983);}
    }else{page("地形信息");h1_screen_id=15;unsigned terrain=sango_battle_terrain_at(g_battle_cursor_x,g_battle_cursor_y);if(terrain<8){sango_battle_read_text(terrain+14,value);paragraph(PAGE_LEFT,64,PAGE_WIDTH,value);}}
    foot("点击或按键返回战场");h1_native_flip();int x,y;next(&x,&y);restore(saved);
}
void sango_battle_show_overview(void){
    Saved saved=save_screen();page("战场全图");h1_screen_id=16;unsigned w=g_battle_map_width,h=g_battle_map_height;int size=PAGE_WIDTH/w;if(size>(int)(180/h))size=180/h;if(size<1)size=1;int ox=(480-w*size)/2,oy=48;
    unsigned map=sango_battle_resource_item(g_battle_map_resource,1),tiles=sango_battle_resource_item(g_battle_tileset_resource,1);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)sprite_pixels((uint8_t *)(tiles+6+((uint8_t *)(map+16))[y*w+x]*64),16,16,0,ox+x*size,oy+y*size,size,size);
    for(int i=0;i<20;i++)if((&g_first_unit_status)[i*7]!=SANGO_UNIT_RETIRED)box(ox+(&g_first_unit_x)[i*7]*size,oy+(&g_first_unit_y)[i*7]*size,size,size,i<10?0xf800:0x001f);
    foot("点击或按键返回战场");h1_native_flip();int x,y;next(&x,&y);restore(saved);
}
