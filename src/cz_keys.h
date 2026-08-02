/* cz_keys.h — CardputerZero CP0キー仕様ヘルパー v1
 * (cardputer.cc /documents/cp0-keys 準拠。全repoに同一コピーをベンダリング — md5一致を保つこと)
 *
 * 提供するもの:
 *  1. cz_keynorm(): テキスト入力モード外で f/z/x/c を fn なしで矢印に、
 *     h をヘルプトグル(CZ_KEY_HELP)に正規化する。
 *     fn+F/Z/X/C はカーネル(keymap2)が既に LV_KEY_UP/... を送ってくるため
 *     LV_KEY_* は素通し = fn 経路と二重発火しない。
 *     fn+H はカーネルで素の 'h' として届くので、'h'=ヘルプで fn+H 要件を満たす。
 *     数字・記号には一切触らない(数値入力系アプリと共存)。
 *  2. cz_help_toggle()/cz_help_key(): 最小のモーダル・ヘルプオーバーレイ。
 *
 * 使い方(main.c からのみ include。key_cb 冒頭に):
 *   uint32_t k = cz_keynorm(lv_event_get_key(e), in_text_mode);
 *   if (cz_help_key(k)) return;                  // ヘルプ表示中は全キー吸収
 *   if (k == CZ_KEY_HELP) { cz_help_toggle(g_root, "タイトル", LINES, N, font); return; }
 *
 * 色は include 前に CZ_HELP_BG 等を #define すれば上書き可。
 */
#ifndef CZ_KEYS_H
#define CZ_KEYS_H

#include <stdbool.h>
#include <stdint.h>

#include <lvgl.h>

#define CZ_KEY_HELP 0xE000u   /* Unicode私用領域: ASCII・LV_KEY_xx・タグ付きコードと非衝突 */

#ifndef CZ_HELP_BG
#define CZ_HELP_BG 0x101820
#define CZ_HELP_FG 0xE0E6EC
#define CZ_HELP_ACC 0x58C0FF
#define CZ_HELP_DIM 0x8090A0
#endif
#ifndef CZ_HELP_HINT
#define CZ_HELP_HINT "fn+H / ESC = close"   /* フォントに日本語が無いアプリ向けの既定 */
#endif

static inline uint32_t cz_keynorm(uint32_t k, int in_text_mode)
{
    if (in_text_mode) return k;
    switch (k) {
    case 'f': case 'F': return LV_KEY_UP;
    case 'z': case 'Z': return LV_KEY_LEFT;
    case 'x': case 'X': return LV_KEY_DOWN;
    case 'c': case 'C': return LV_KEY_RIGHT;
    case 'h': case 'H': return CZ_KEY_HELP;
    }
    return k;
}

/* ---- ヘルプオーバーレイ ---- */

static lv_obj_t *cz__help __attribute__((unused));

static inline void cz__help_on_delete(lv_event_t *e)
{
    (void)e;
    cz__help = NULL;               /* 画面遷移の lv_obj_clean() で消えても追従 */
}

static inline bool cz_help_visible(void) { return cz__help != NULL; }

static inline void cz_help_toggle(lv_obj_t *parent, const char *title,
                           const char *const *lines, int nlines,
                           const lv_font_t *font)
{
    if (cz__help) { lv_obj_delete(cz__help); return; }   /* DELETE cb が NULL に戻す */

    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, 320, 170);
    lv_obj_set_pos(p, 0, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(CZ_HELP_BG), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_color(p, lv_color_hex(CZ_HELP_ACC), 0);
    lv_obj_set_style_pad_all(p, 6, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(p, cz__help_on_delete, LV_EVENT_DELETE, NULL);

    lv_obj_t *t = lv_label_create(p);
    if (font) lv_obj_set_style_text_font(t, font, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(CZ_HELP_ACC), 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 2, 0);

    int y = 18;
    for (int i = 0; i < nlines; i++, y += 14) {
        lv_obj_t *l = lv_label_create(p);
        if (font) lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(CZ_HELP_FG), 0);
        lv_label_set_text(l, lines[i]);
        lv_obj_set_pos(l, 2, y);
    }

    lv_obj_t *hint = lv_label_create(p);
    if (font) lv_obj_set_style_text_font(hint, font, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(CZ_HELP_DIM), 0);
    lv_label_set_text(hint, CZ_HELP_HINT);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -2, 0);

    cz__help = p;
}

/* ヘルプ表示中は全キーを吸収(モーダル)。ESC / fn+H(='h') で閉じる。
 * true を返したら key_cb は即 return すること。 */
static inline bool cz_help_key(uint32_t k)
{
    if (!cz__help) return false;
    if (k == LV_KEY_ESC || k == CZ_KEY_HELP) lv_obj_delete(cz__help);
    return true;
}

#endif /* CZ_KEYS_H */
