/* port/main.c — standalone-binary entry for the CardputerZero AppStore build.
 *
 * The AppStore fork/execs a standalone binary that owns the display + keyboard.
 * This main() sets up an LVGL display + input and calls the app ABI
 * app_main(lv_screen_active()) / app_event(), so src/ is reused as-is. The
 * GStreamer reproduction (started on Enter) runs on its own background thread.
 *
 *   default        : off-screen memory display — headless build/run check
 *                    (app prints "camstreamemu-ui: ready" which build.sh greps)
 *   -DPORT_FBDEV   : lv_linux_fbdev + lv_evdev — the on-device build
 *
 * Exit: SIGTERM (Home / long-ESC) -> CZ_EV_EXIT_REQUEST -> clean teardown. */
#include <lvgl.h>
#include <cz_app.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define LCD_W 320
#define LCD_H 170

static volatile sig_atomic_t g_quit;
static void on_term(int s) { (void)s; g_quit = 1; }

static uint32_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

#if !defined(PORT_FBDEV)
static void null_flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a; (void)px;
    lv_display_flush_ready(d);
}
#endif

int main(void)
{
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);

    lv_init();

#if defined(PORT_FBDEV)
    lv_display_t *disp = lv_linux_fbdev_create();
    const char *fb = getenv("LV_LINUX_FBDEV_DEVICE");
    lv_linux_fbdev_set_file(disp, fb && *fb ? fb : "/dev/fb1");
    extern lv_indev_t *evdev_kbd_create(const char *dev);
    const char *kbd = getenv("APP_KEY_INPUT_DEVICE");
    lv_indev_t *in = evdev_kbd_create(kbd && *kbd ? kbd : "/dev/input/event0");
    (void)in;
#else
    static uint8_t buf[LCD_W * LCD_H * 2];          /* RGB565 */
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_flush_cb(disp, null_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
#endif
    (void)disp;

    app_main(lv_screen_active());

    uint32_t t = now_ms();
    while (!g_quit) {
        uint32_t n = now_ms();
        lv_tick_inc(n - t); t = n;
        lv_timer_handler();
        usleep(5000);
    }

    app_event(CZ_EV_EXIT_REQUEST, NULL);
    return 0;
}
