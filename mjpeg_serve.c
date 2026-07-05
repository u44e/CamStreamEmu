/* mjpeg_serve.c — see mjpeg_serve.h. A tiny HTTP server that answers GET with a
 * multipart/x-mixed-replace JPEG stream fed by a GStreamer jpegenc appsink. */
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include "mjpeg_serve.h"
#include "repro.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define BOUNDARY "camemuframe"

typedef struct {
    int listen_fd;
    _Atomic int client_fd;       /* current connected client, -1 if none */
    pthread_mutex_t wlock;
} mjpeg_ctx;

static const char *env_or(const char *k, const char *d)
{ const char *v = getenv(k); return (v && *v) ? v : d; }

/* accept loop: on each new client, send the HTTP + multipart preamble */
static void *accept_thread(void *arg)
{
    mjpeg_ctx *c = (mjpeg_ctx *)arg;
    for (;;) {
        int fd = accept(c->listen_fd, NULL, NULL);
        if (fd < 0) break;
        char req[1024];
        recv(fd, req, sizeof req, 0);          /* consume the GET request */
        const char *hdr =
            "HTTP/1.0 200 OK\r\n"
            "Cache-Control: no-cache\r\n"
            "Pragma: no-cache\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=" BOUNDARY "\r\n\r\n";
        if (send(fd, hdr, strlen(hdr), MSG_NOSIGNAL) < 0) { close(fd); continue; }
        int old = atomic_exchange(&c->client_fd, fd);
        if (old >= 0) close(old);              /* one viewer at a time */
        fprintf(stderr, "mjpeg: client connected\n");
    }
    return NULL;
}

static GstFlowReturn on_jpeg(GstAppSink *sink, gpointer user)
{
    mjpeg_ctx *c = (mjpeg_ctx *)user;
    GstSample *s = gst_app_sink_pull_sample(sink);
    if (!s) return GST_FLOW_OK;
    int fd = atomic_load(&c->client_fd);
    GstBuffer *b = gst_sample_get_buffer(s);
    GstMapInfo m;
    if (fd >= 0 && b && gst_buffer_map(b, &m, GST_MAP_READ)) {
        char part[128];
        int hl = snprintf(part, sizeof part,
            "--" BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
            (size_t)m.size);
        pthread_mutex_lock(&c->wlock);
        int ok = send(fd, part, hl, MSG_NOSIGNAL) >= 0 &&
                 send(fd, m.data, m.size, MSG_NOSIGNAL) >= 0 &&
                 send(fd, "\r\n", 2, MSG_NOSIGNAL) >= 0;
        pthread_mutex_unlock(&c->wlock);
        atomic_fetch_add_explicit(&g_repro_bytes, hl + m.size + 2, memory_order_relaxed);
        atomic_fetch_add_explicit(&g_repro_packets, 1, memory_order_relaxed);
        if (!ok) { close(fd); atomic_store(&c->client_fd, -1); fprintf(stderr, "mjpeg: client gone\n"); }
        gst_buffer_unmap(b, &m);
    }
    gst_sample_unref(s);
    return GST_FLOW_OK;
}

int mjpeg_serve_run(const cam_profile_t *p)
{
    signal(SIGPIPE, SIG_IGN);
    gst_init(NULL, NULL);

    static mjpeg_ctx c;
    memset(&c, 0, sizeof c);
    atomic_store(&c.client_fd, -1);
    pthread_mutex_init(&c.wlock, NULL);

    int port = p->server_port > 0 ? p->server_port : 80;
    c.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; setsockopt(c.listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(port);
    if (bind(c.listen_fd, (struct sockaddr *)&a, sizeof a) < 0) {
        fprintf(stderr, "mjpeg: cannot bind port %d (privileged? try 8080)\n", port); return -1;
    }
    listen(c.listen_fd, 4);

    pthread_t th; pthread_create(&th, NULL, accept_thread, &c);

    int w = p->width > 0 ? p->width : 1280, h = p->height > 0 ? p->height : 720;
    int fps = p->fps > 0 ? (int)(p->fps + 0.5) : 15;
#if defined(__linux__)
    const char *src = env_or("CAMEMU_SRC", "libcamerasrc");
#else
    const char *src = env_or("CAMEMU_SRC", "videotestsrc is-live=true");
#endif
    char pipe[1024];
    snprintf(pipe, sizeof pipe,
        "%s ! video/x-raw,width=%d,height=%d,framerate=%d/1 ! videoconvert ! jpegenc "
        "! appsink name=out sync=true max-buffers=2 drop=true", src, w, h, fps);
    GError *err = NULL;
    GstElement *pl = gst_parse_launch(pipe, &err);
    if (!pl || err) { fprintf(stderr, "mjpeg: pipeline: %s\n", err ? err->message : "?");
                      if (err) g_error_free(err); return -1; }
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pl), "out");
    GstAppSinkCallbacks cb = { 0 };
    cb.new_sample = on_jpeg;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &cb, &c, NULL);

    const char *path = "/";
    if (p->url[0]) { const char *s = strstr(p->url, "://"); s = s ? strchr(s + 3, '/') : NULL; if (s) path = s; }
    gst_element_set_state(pl, GST_STATE_PLAYING);
    fprintf(stderr, "mjpeg: serving http://0.0.0.0:%d%s (%dx%d %dfps JPEG)\n", port, path, w, h, fps);

    GstBus *bus = gst_element_get_bus(pl);
    while (!atomic_load_explicit(&g_repro_stop, memory_order_acquire)) {
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, 200 * GST_MSECOND,
                                                     GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
        if (msg) { gst_message_unref(msg); break; }
    }
    gst_object_unref(bus);
    gst_element_set_state(pl, GST_STATE_NULL);
    gst_object_unref(sink); gst_object_unref(pl);
    close(c.listen_fd);
    return 0;
}
