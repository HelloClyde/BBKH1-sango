#include "sango_internal.h"

/* 粮食、金钱、兵力输入面板。 */

/* 原函数 0x81c13b9c */
uint32_t sango_ui_input_food_amount(uint32_t value, uint32_t maximum_amount)
{
#ifndef SANGO_H1
    char prompt_text[24];
    /* Only the legacy number dialog completes this paint scope. */
    sango_ui_paint_begin();
    sango_display_fill_gray_rectangle(0x53, 0x8d, 0x9b, 0xb1, 0, 0);
    sango_display_draw_rectangle(0x52, 0x8c, 0x9c, 0xb2, 3, 0, 0);
    sango_resource_read_text(0x40, 0x29, prompt_text);
    sango_text_draw_legacy(0x53, 0x8d, prompt_text);
#endif
    return sango_ui_select_number(value & 0xffff, maximum_amount & 0xffff);
}

/* 原函数 0x81c13c3c */
uint32_t sango_ui_input_money_amount(uint32_t value, uint32_t maximum_amount)
{
#ifndef SANGO_H1
    char prompt_text[24];
    sango_ui_paint_begin();
    sango_display_fill_gray_rectangle(0x53, 0x8d, 0x9b, 0xb1, 0, 0);
    sango_display_draw_rectangle(0x52, 0x8c, 0x9c, 0xb2, 3, 0, 0);
    sango_resource_read_text(0x40, 0x2a, prompt_text);
    sango_text_draw_legacy(0x53, 0x8d, prompt_text);
#endif
    return sango_ui_select_number(value & 0xffff, maximum_amount & 0xffff);
}

/* 原函数 0x81c13cdc */
uint32_t sango_ui_input_troop_amount(uint32_t value, uint32_t maximum_amount)
{
#ifndef SANGO_H1
    char prompt_text[24];
    sango_ui_paint_begin();
    sango_display_fill_gray_rectangle(0x53, 0x8d, 0x9b, 0xb1, 0, 0);
    sango_display_draw_rectangle(0x52, 0x8c, 0x9c, 0xb2, 3, 0, 0);
    sango_resource_read_text(0x40, 0x2b, prompt_text);
    sango_text_draw_legacy(0x53, 0x8d, prompt_text);
#endif
    return sango_ui_select_number(value & 0xffff, maximum_amount & 0xffff);
}
