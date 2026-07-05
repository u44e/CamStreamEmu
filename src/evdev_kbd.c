/* port/evdev_kbd.c — evdev keyboard for the standalone build.
 *
 * The hub/node UI mainly reacts to the physical SIDE button (switch view), which
 * the dlopen host delivered as app_event(CZ_EV_SIDE_KEY). This LVGL keypad indev
 * reads a raw Linux evdev keyboard, fires CZ_EV_SIDE_KEY on the SIDE key, and
 * forwards plain nav/printable keys to LVGL. The SIDE keycode is device-specific
 * (override with -DAPP_SIDE_KEYCODE=<n>); KEY_MENU is a placeholder pending the
 * CardputerZero keymap. */
#include <lvgl.h>
#include <cz_app.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>

#ifndef APP_SIDE_KEYCODE
#define APP_SIDE_KEYCODE KEY_MENU
#endif

typedef struct {
    int      fd;
    int      shift;
    uint32_t q[16]; int qh, qt;
    int      releasing;
} evdev_kbd_t;

static uint32_t decode(int code)
{
    switch (code) {
    case KEY_UP:    return LV_KEY_UP;
    case KEY_DOWN:  return LV_KEY_DOWN;
    case KEY_LEFT:  return LV_KEY_LEFT;
    case KEY_RIGHT: return LV_KEY_RIGHT;
    case KEY_ENTER: case KEY_KPENTER: return LV_KEY_ENTER;
    case KEY_ESC:       return LV_KEY_ESC;
    case KEY_BACKSPACE: return LV_KEY_BACKSPACE;
    case KEY_TAB:       return LV_KEY_NEXT;
    }
    return 0;
}

static void pump(evdev_kbd_t *k)
{
    struct input_event ev;
    while (read(k->fd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
        if (ev.type != EV_KEY || ev.value == 0) continue;   /* press/repeat only */
        if (ev.code == APP_SIDE_KEYCODE) { app_event(CZ_EV_SIDE_KEY, NULL); continue; }
        uint32_t key = decode(ev.code);
        if (key) {
            int nt = (k->qt + 1) & 15;
            if (nt != k->qh) { k->q[k->qt] = key; k->qt = nt; }
        }
    }
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    evdev_kbd_t *k = lv_indev_get_user_data(indev);
    if (k->releasing) { k->releasing = 0; data->state = LV_INDEV_STATE_RELEASED; return; }
    pump(k);
    if (k->qh != k->qt) {
        data->key = k->q[k->qh]; k->qh = (k->qh + 1) & 15;
        data->state = LV_INDEV_STATE_PRESSED;
        k->releasing = 1;
        data->continue_reading = (k->qh != k->qt);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

lv_indev_t *evdev_kbd_create(const char *dev)
{
    static evdev_kbd_t k;
    memset(&k, 0, sizeof k);
    k.fd = open(dev, O_RDONLY | O_NONBLOCK);
    if (k.fd < 0) return NULL;
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, read_cb);
    lv_indev_set_user_data(indev, &k);
    return indev;
}
