typedef unsigned int bda_handle_t;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef struct {unsigned int words[8];} bda_gui_message_t;
typedef struct {int width,height; const uint16_t *source_pixels; int selected_index;} bda_gui_picture_t;
enum { BDA_KEY_UP=0x67,BDA_KEY_DOWN=0x6c,BDA_KEY_LEFT=0x69,BDA_KEY_RIGHT=0x6a,
 BDA_KEY_ENTER=0x1c,BDA_KEY_ESCAPE=1,BDA_MSG_WINDOW_TIMER=0x144,
 BDA_MSG_TOUCH_COORDINATE=1,BDA_MSG_TOUCH_RELEASE=2,BDA_SEEK_SET=0,BDA_SEEK_END=2 };
extern int h1_layout_game;
extern int h1_native_active;
extern unsigned int h1_screen_id;
extern uint16_t h1_native_pixels[480*272];
void h1_native_flip(void);
void h1_native_text(int,int,const char *,int,unsigned int);
int h1_ui_assets_load(void);
void h1_ui_assets_free(void);
void h1_ui_asset(unsigned int,int,int);
void h1_native_battle_hud(void);
void h1_compose_battle(void);
void h1_strategy_hud(void);
void *h1_compat_memset(void *,int,unsigned int);
int bda_gui_event_pump_frame_once(bda_gui_message_t *,unsigned int);
void bda_gui_touch_position(uint16_t *,uint16_t *);
unsigned int bda_gui_tick_count_25ms(void);
int bda_gui_window_timer_start(unsigned int,unsigned int,unsigned int);
int bda_gui_window_timer_stop(unsigned int,unsigned int);
unsigned int bda_gui_window_timer_clock_ms(void);
void bda_gui_render_picture(unsigned int,int,int,int,int,const bda_gui_picture_t *);
int bda_fs_fopen_raw(const char *,const char *);
int bda_fs_file_is_valid(int);
int bda_fs_close_raw(int);
int bda_fs_read_raw(int,void *,unsigned int);
int bda_fs_write_raw(int,const void *,unsigned int);
int bda_fs_seek_raw(int,int,int);
void bda_fs_flush_all(void);
void *bda_gui_select_draw_object(unsigned int,void *);
#define bda_gui_put_pixel bda_sango_gui_put_pixel
#define bda_gui_set_text_mode bda_sango_gui_set_text_mode
#define bda_gui_set_text_color bda_sango_gui_set_text_color
#define bda_gui_rgb bda_sango_gui_rgb
#define bda_gui_draw_text bda_sango_gui_draw_text
#define bda_gui_context_copy bda_sango_gui_context_copy
#define bda_gui_compatible_context_create bda_sango_gui_compatible_create
#define bda_gui_compatible_context_free bda_sango_gui_compatible_free
#define bda_gui_draw_object_create bda_sango_gui_draw_object
#define bda_gui_move_to bda_sango_gui_move_to
#define bda_gui_line_to bda_sango_gui_line_to
#define bda_gui_rectangle bda_sango_gui_rectangle
#define bda_gui_rgb565_avoid_black_key(c) ((c)?(c):1)
