#include "sango_internal.h"
#include "h1_sdk.h"
#include "audio.h"
int sango_music_sdk_file_open(const char *p){return bda_fs_fopen_raw(p,"rb");}
void sango_music_sdk_file_close(int f){bda_fs_close_raw(f);}
int sango_music_sdk_file_read(int f,void *p,unsigned n){return bda_fs_read_raw(f,p,n);}
int sango_music_sdk_file_seek(int f,int o,int w){return bda_fs_seek_raw(f,o,w);}
uint32_t sango_music_sdk_clock(void){return bda_gui_window_timer_clock_ms();}
void sango_music_sdk_timer(int active){if(active)bda_gui_window_timer_start(g_app_frame,SANGO_MUSIC_TIMER_ID,SANGO_MUSIC_POLL_MS);else bda_gui_window_timer_stop(g_app_frame,SANGO_MUSIC_TIMER_ID);}
void sango_music_sdk_audio_open(void){gba_audio_open(22050);gba_audio_set_volume(sango_music_get_volume()*10);}
void sango_music_sdk_audio_volume(unsigned n){gba_audio_set_volume(n*10);}
int sango_music_sdk_audio_ready(void){return !gba_audio_wait();}
int sango_music_sdk_audio_write(const void *p,unsigned n){static int16_t stereo[1024];const int16_t *mono=p;unsigned count=n/2;if(count>512)count=512;for(unsigned i=0;i<count;i++)stereo[2*i]=stereo[2*i+1]=mono[i];gba_audio_write(stereo,count);return count*2;}
void sango_music_sdk_audio_stop(void){gba_audio_close();}
