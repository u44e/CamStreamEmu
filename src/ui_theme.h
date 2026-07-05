/* ui_theme.h — shared palette + tiny LVGL helpers (parent-explicit so every
 * ui_*.c can use them). Palette follows the NetTerm/location-tracker family
 * look, proven readable on the 320x170 LCD. */
#ifndef PS_UI_THEME_H
#define PS_UI_THEME_H

#include <lvgl.h>

#define LCD_W 320
#define LCD_H 170
#define TITLE_H 22                   /* top title band (NetTerm style, no fill) */
#define SBAR_H 16                    /* bottom status bar */
#define HDR_H  12                    /* column header row (below the title band) */
#define BODY_ROWS 10                 /* 12px mono rows between header and status bar */
#define BODY_TOP (TITLE_H + HDR_H)   /* y of the first body row on list screens */

#define COL_BG     0x1A1A2E
#define COL_TITLE  0x3AD8FF
#define COL_TEXT   0xECECF2
#define COL_DIM    0xA2A2C0
#define COL_HILITE 0x2C2C52
#define COL_CYAN   0x3AD8FF
#define COL_AMBER  0xFFB82E
#define COL_GREEN  0x4CD96A
#define COL_RED    0xFF6B6B
#define COL_SBAR   0x10101E

/* per-protocol row tint (list + tree), Wireshark-ish but dark-theme */
#define COL_P_TCP   0xB9C7FF
#define COL_P_UDP   0xBFE8FF
#define COL_P_ARP   0xD9C79A
#define COL_P_ICMP  0xFFC7E0
#define COL_P_DNS   0xC9F0C9
#define COL_P_RTP   0x9FE8C8
#define COL_P_TS    0x7FE0A8
#define COL_P_RTSP  0xFFD98A
#define COL_P_ERR   0xFF6B6B

static inline lv_obj_t *ps_label(lv_obj_t *parent, const lv_font_t *f,
                                 uint32_t color, int x, int y, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, txt);
    return l;
}

static inline lv_obj_t *ps_rect(lv_obj_t *parent, uint32_t color,
                                int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    return o;
}

#endif /* PS_UI_THEME_H */
