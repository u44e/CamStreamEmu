/*
 * CamStreamEmu — CardputerZero LVGL UI.
 *
 *   SCR_LIST   browse camera-profile JSONs (samples / SD card), show each
 *              camera's codec/res/fps/delivery at a glance
 *   SCR_RUN    reproduce the selected camera: live delivery mode / dest /
 *              packets / bytes / elapsed; ESC or 's' stops
 *
 * App ABI: app_main(parent)/app_event(). GStreamer reproduction runs on a
 * background thread (repro.c); a 250 ms lv_timer polls the stats.
 * Env: CSE_DIR extra profile dir (emu). Test hooks (-DPS_TEST_HOOKS): AUTO_SEL,
 * AUTO_SCREEN=run for headless EMU_SHOT.
 */
#include <cz_app.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#include "ui_theme.h"
#include "../profile.h"
#include "../repro.h"

#define CSE_VERSION "0.1.0"

#if defined(APP_EMU)
#define SAMPLE_DIR "samples"
#else
#define SAMPLE_DIR "/usr/share/APPLaunch/share/samples"
#endif

enum { SCR_LIST, SCR_RUN };

static lv_obj_t   *g_root;
static int         g_scr = SCR_LIST;
static lv_group_t *g_grp;
static lv_obj_t   *g_cap;
static lv_timer_t *g_timer;
static const lv_font_t *F14, *F12;

#define MAXP 64
static char  g_names[MAXP][64];
static char  g_paths[MAXP][512];
static int   g_np, g_sel;
static cam_profile_t g_cur;
static lv_obj_t *g_stat[6];

void key_cb(lv_event_t *e);

/* title-bar top-right cluster (clock / LAN / battery), NetTerm style — matches
 * PacketScope. CardputerZero is wired-Ethernet: a small rounded RJ45 plug icon
 * (body + slot + clip) shows link status, not wifi. */
static lv_obj_t *g_sb_time, *g_sb_batt, *g_sb_lan[3];
static int       s_batt = -1, s_lan = -1;

static int read_battery(void)
{
#if defined(APP_EMU)
    FILE *fp = popen("pmset -g batt 2>/dev/null", "r");
    if (!fp) return -1;
    char line[256]; int pct = -1;
    while (fgets(line, sizeof line, fp)) {
        char *pc = strchr(line, '%');
        if (pc) { char *s = pc; while (s > line && isdigit((unsigned char)s[-1])) s--; pct = atoi(s); break; }
    }
    pclose(fp);
    return pct;
#else
    const char *paths[] = { "/sys/class/power_supply/BAT0/capacity",
                            "/sys/class/power_supply/BAT1/capacity",
                            "/sys/class/power_supply/battery/capacity" };
    for (int i = 0; i < 3; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) { int v = -1, ok = fscanf(f, "%d", &v); fclose(f); if (ok == 1) return v; }
    }
    return -1;
#endif
}

static int read_lan(void)   /* 1 up, 0 down, -1 unknown — wired Ethernet */
{
#if defined(APP_EMU)
    return 1;
#else
    const char *ifs[] = { "eth0", "end0", "enu1u1", "usb0", "eth1" };
    for (int i = 0; i < 5; i++) {
        char p[64]; snprintf(p, sizeof p, "/sys/class/net/%s/carrier", ifs[i]);
        FILE *f = fopen(p, "r");
        if (f) { int v = -1, ok = fscanf(f, "%d", &v); fclose(f); if (ok == 1) return v ? 1 : 0; }
    }
    return -1;
#endif
}

static void statuscluster_set(void)
{
    if (!g_sb_time) return;
    time_t t = time(NULL); struct tm m; localtime_r(&t, &m);
    char tb[8]; snprintf(tb, sizeof tb, "%02d:%02d", m.tm_hour, m.tm_min);
    lv_label_set_text(g_sb_time, tb);

    static time_t last = 0;
    if (last == 0 || t - last >= 20) { last = t; s_batt = read_battery(); s_lan = read_lan(); }

    char bb[16];
    if (s_batt >= 0) snprintf(bb, sizeof bb, "%d%%", s_batt);
    else             snprintf(bb, sizeof bb, "--%%");
    lv_label_set_text(g_sb_batt, bb);
    lv_obj_set_style_text_color(g_sb_batt, lv_color_hex(s_batt >= 0 && s_batt < 15 ? COL_RED : COL_TEXT), 0);

    uint32_t lc = s_lan == 1 ? COL_GREEN : s_lan == 0 ? COL_RED : COL_DIM;
    lv_obj_set_style_bg_color(g_sb_lan[0], lv_color_hex(lc), 0);
    lv_obj_set_style_bg_color(g_sb_lan[2], lv_color_hex(lc), 0);

    /* right-align the cluster: [clock]  [plug]  [battery%] */
    int rx = LCD_W - 6;
    int bx = rx - (int)strlen(bb) * 8;
    lv_obj_set_pos(g_sb_batt, bx, 4);
    int lx = bx - 9 - 12, ly = 4;
    lv_obj_set_pos(g_sb_lan[2], lx + 4, ly);      lv_obj_set_size(g_sb_lan[2], 4, 2);   /* clip */
    lv_obj_set_pos(g_sb_lan[0], lx, ly + 2);      lv_obj_set_size(g_sb_lan[0], 12, 10); /* body */
    lv_obj_set_pos(g_sb_lan[1], lx + 2, ly + 8);  lv_obj_set_size(g_sb_lan[1], 8, 4);   /* pin slot */
    lv_obj_set_pos(g_sb_time, lx - 7 - 30, 4);                                          /* clock */
}

/* NetTerm-style title band: plain (no fill), font-14 title on the left, a
 * clock / LAN / battery cluster on the right. */
static void titlebar(const char *title)
{
    ps_label(g_root, F14, COL_TITLE, 8, 3, title && *title ? title : "CamStreamEmu");
    g_sb_time = ps_label(g_root, F12, COL_DIM, 0, 4, "");
    for (int i = 0; i < 3; i++) g_sb_lan[i] = ps_rect(g_root, COL_GREEN, 0, 0, 4, 4);   /* RJ45 plug */
    lv_obj_set_style_radius(g_sb_lan[0], 2, 0);
    lv_obj_set_style_bg_color(g_sb_lan[1], lv_color_hex(COL_BG), 0);                    /* slot = bg */
    g_sb_batt = ps_label(g_root, F12, COL_TEXT, 0, 4, "");
    statuscluster_set();
    ps_rect(g_root, COL_CYAN, 0, TITLE_H - 1, LCD_W, 1);
}

static void attach_capture(void)
{
    if (g_grp) { lv_group_delete(g_grp); g_grp = NULL; }
    g_cap = lv_obj_create(g_root);
    lv_obj_set_size(g_cap, 1, 1); lv_obj_set_pos(g_cap, -10, -10);
    lv_obj_clear_flag(g_cap, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_cap, key_cb, LV_EVENT_KEY, NULL);
    g_grp = lv_group_create();
    for (lv_indev_t *id = lv_indev_get_next(NULL); id; id = lv_indev_get_next(id))
        if (lv_indev_get_type(id) == LV_INDEV_TYPE_KEYPAD) lv_indev_set_group(id, g_grp);
    lv_group_add_obj(g_grp, g_cap);
    lv_group_focus_obj(g_cap);
}

static void clear_root(void) { lv_obj_clean(g_root); g_sb_time = NULL; attach_capture(); }

/* ---- profile list ---- */
static void scan(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_np < MAXP) {
        const char *dot = strrchr(e->d_name, '.');
        if (!dot || strcmp(dot, ".json")) continue;
        snprintf(g_paths[g_np], sizeof g_paths[0], "%s/%s", dir, e->d_name);
        snprintf(g_names[g_np], sizeof g_names[0], "%s", e->d_name);
        g_np++;
    }
    closedir(d);
}

static void row_label(int i, char *out, size_t n)
{
    cam_profile_t p;
    if (cam_profile_load(g_paths[i], &p) == 0 && p.codec[0])
        snprintf(out, n, "%-22s %s %dx%d", g_names[i], p.codec, p.width, p.height);
    else
        snprintf(out, n, "%s", g_names[i]);
}

static void show_list(void)
{
    g_scr = SCR_LIST;
    clear_root();
    titlebar("CamStreamEmu");
    ps_label(g_root, F12, COL_CYAN, 6, TITLE_H, "camera profiles  (Enter: reproduce)");
    int top = g_sel - 4; if (top < 0) top = 0;
    for (int i = 0; i < BODY_ROWS && top + i < g_np; i++) {
        int idx = top + i, y = TITLE_H + 14 + i * 12;
        if (idx == g_sel) ps_rect(g_root, COL_HILITE, 0, y, LCD_W, 12);
        char l[96]; row_label(idx, l, sizeof l);
        ps_label(g_root, F12, idx == g_sel ? COL_CYAN : COL_TEXT, 6, y, l);
    }
    ps_rect(g_root, COL_SBAR, 0, LCD_H - SBAR_H, LCD_W, SBAR_H);
    char sbb[48]; snprintf(sbb, sizeof sbb, "v%s  %d profiles  Up/Dn Enter", CSE_VERSION, g_np);
    ps_label(g_root, F12, COL_DIM, 4, LCD_H - SBAR_H + 2, sbb);
    if (g_np == 0)
        ps_label(g_root, F12, COL_AMBER, 6, TITLE_H + 20, "No .json profiles found.");
}

/* ---- reproduce ---- */
static void show_run(void)
{
    g_scr = SCR_RUN;
    clear_root();
    titlebar(g_cur.codec);
    char l[80];
    snprintf(l, sizeof l, "%s  %s  %dx%d @%.2f", g_cur.codec, g_cur.container,
             g_cur.width, g_cur.height, g_cur.fps);
    ps_label(g_root, F12, COL_TEXT, 6, TITLE_H + 2, l);
    snprintf(l, sizeof l, "GOP %d  %ldkbps  PT %d", g_cur.gop, g_cur.bitrate_kbps, g_cur.payload_type);
    ps_label(g_root, F12, COL_DIM, 6, TITLE_H + 16, l);
    for (int i = 0; i < 4; i++)
        g_stat[i] = ps_label(g_root, F12, COL_GREEN, 6, TITLE_H + 34 + i * 13, "");
    ps_rect(g_root, COL_SBAR, 0, LCD_H - SBAR_H, LCD_W, SBAR_H);
    ps_label(g_root, F12, COL_DIM, 4, LCD_H - SBAR_H + 2, "s / ESC : stop");
}

static void run_tick(void)
{
    if (g_scr != SCR_RUN) return;
    repro_stats s; repro_get(&s);
    char l[64];
    snprintf(l, sizeof l, "mode  : %s", s.mode);            lv_label_set_text(g_stat[0], l);
    snprintf(l, sizeof l, "dest  : %s", s.dest);            lv_label_set_text(g_stat[1], l);
    snprintf(l, sizeof l, "sent  : %u pkt / %llu KB", s.packets,
             (unsigned long long)(s.bytes / 1024));         lv_label_set_text(g_stat[2], l);
    snprintf(l, sizeof l, "time  : %us  %s", s.elapsed_s, s.running ? "streaming" : "stopped");
    lv_label_set_text(g_stat[3], l);
    lv_obj_set_style_text_color(g_stat[3], lv_color_hex(s.running ? COL_GREEN : COL_DIM), 0);
}

static void start_selected(void)
{
    if (g_sel >= g_np) return;
    if (cam_profile_load(g_paths[g_sel], &g_cur) != 0) return;
    show_run();
    repro_start(&g_cur, 0);
    run_tick();
}

static void stop_and_back(void)
{
    repro_stop();
    show_list();
}

void key_cb(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    if (g_scr == SCR_LIST) {
        if (k == LV_KEY_UP && g_sel > 0) { g_sel--; show_list(); }
        else if (k == LV_KEY_DOWN && g_sel < g_np - 1) { g_sel++; show_list(); }
        else if (k == LV_KEY_ENTER && g_np > 0) start_selected();
    } else {
        if (k == 's' || k == 'S' || k == LV_KEY_ESC) stop_and_back();
    }
}

static void on_tick(lv_timer_t *t) { (void)t; statuscluster_set(); run_tick(); }

CZ_APP_EXPORT void app_main(lv_obj_t *parent)
{
    g_root = parent;
    lv_obj_set_style_bg_color(g_root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
    F14 = &lv_font_montserrat_14; F12 = &lv_font_montserrat_12;

    g_np = 0;
    scan(SAMPLE_DIR);
    const char *ex = getenv("CSE_DIR"); if (ex) scan(ex);
    scan("/sdcard");

    g_timer = lv_timer_create(on_tick, 250, NULL);

#if defined(PS_TEST_HOOKS)
    const char *sel = getenv("AUTO_SEL"); if (sel) g_sel = atoi(sel);
    const char *scr = getenv("AUTO_SCREEN");
    if (scr && !strcmp(scr, "run")) { start_selected(); }
    else show_list();
#else
    show_list();
#endif
    printf("camstreamemu-ui: ready\n"); fflush(stdout);
}

CZ_APP_EXPORT void app_event(int type, void *data)
{
    (void)data;
    if (type == CZ_EV_EXIT_REQUEST) repro_stop();
}

#if defined(APP_EMU)
void ui_init(void) { app_main(lv_screen_active()); }
#endif
