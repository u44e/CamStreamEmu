/* repro.c — see repro.h. Runs one of the blocking delivery entry points on a
 * background thread, tracking elapsed time; the delivery modules bump the shared
 * byte/packet counters and poll g_repro_stop to exit. */
#include "repro.h"
#include "video_pipe.h"
#include "rtsp_serve.h"
#include "mjpeg_serve.h"
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static struct {
    pthread_t th;
    int       started;
    cam_profile_t p;
    int       mode;            /* resolved: 1 rtsp, 2 mjpeg, 3 multicast */
    _Atomic long start_s;
    char      dest[48];
} R;

static long now_s(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec;
}

static void *worker(void *arg)
{
    (void)arg;
    if (R.mode == 1)      rtsp_serve_run(&R.p);
    else if (R.mode == 2) mjpeg_serve_run(&R.p);
    else                  video_pipe_run(&R.p);
    return NULL;
}

int repro_start(const cam_profile_t *p, int mode)
{
    if (R.started) return -1;
    R.p = *p;
    if (mode == 0) {         /* auto: pick by the profile's control/container */
        if (!strcmp(p->ctrl_proto, "http-mjpeg") || !strcmp(p->container, "http-mjpeg")) mode = 2;
        else if (!strcmp(p->ctrl_proto, "rtsp")) mode = 1;
        else mode = 3;
    }
    R.mode = mode;
    if (mode == 3)
        snprintf(R.dest, sizeof R.dest, "%s:%d", p->dst_ip[0] ? p->dst_ip : "239.x", p->dst_port);
    else {
        int port = p->server_port > 0 ? p->server_port : (mode == 1 ? 554 : 80);
        snprintf(R.dest, sizeof R.dest, "%s :%d", mode == 1 ? "rtsp" : "http", port);
    }

    atomic_store(&g_repro_bytes, 0);
    atomic_store(&g_repro_packets, 0);
    atomic_store(&g_repro_stop, 0);
    atomic_store(&R.start_s, now_s());
    if (pthread_create(&R.th, NULL, worker, NULL) != 0) return -1;
    R.started = 1;
    return 0;
}

void repro_stop(void)
{
    if (!R.started) return;
    atomic_store_explicit(&g_repro_stop, 1, memory_order_release);
    pthread_join(R.th, NULL);
    R.started = 0;
}

int repro_running(void) { return R.started; }

void repro_get(repro_stats *out)
{
    memset(out, 0, sizeof *out);
    out->running = R.started;
    out->bytes = atomic_load_explicit(&g_repro_bytes, memory_order_relaxed);
    out->packets = atomic_load_explicit(&g_repro_packets, memory_order_relaxed);
    out->elapsed_s = R.started ? (uint32_t)(now_s() - atomic_load(&R.start_s)) : 0;
    snprintf(out->mode, sizeof out->mode, "%s",
             R.mode == 1 ? "RTSP" : R.mode == 2 ? "MJPEG" : "multicast");
    snprintf(out->dest, sizeof out->dest, "%s", R.dest);
}
