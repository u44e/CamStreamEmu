/* video_pipe.c — see video_pipe.h. */
#include "video_pipe.h"
#include "repro.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* shared reproduction stats/stop (declared extern in repro.h); defined here so
 * both the CLI and the UI link them regardless of which entry points they use */
_Atomic uint64_t g_repro_bytes   = 0;
_Atomic uint32_t g_repro_packets = 0;
_Atomic int      g_repro_stop    = 0;

/* count each buffer leaving the udpsink for the UI's live stats */
static GstPadProbeReturn byte_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u)
{
    (void)pad; (void)u;
    GstBuffer *b = GST_PAD_PROBE_INFO_BUFFER(info);
    if (b) {
        atomic_fetch_add_explicit(&g_repro_bytes, gst_buffer_get_size(b), memory_order_relaxed);
        atomic_fetch_add_explicit(&g_repro_packets, 1, memory_order_relaxed);
    }
    return GST_PAD_PROBE_OK;
}

/* run a built pipeline until ERROR/EOS or g_repro_stop; attaches the byte probe
 * to the element named "sink" if present. Shared by CLI and UI multicast paths. */
int video_pipe_spin(void *pipeline_v)
{
    GstElement *pipeline = (GstElement *)pipeline_v;
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (sink) {
        GstPad *sp = gst_element_get_static_pad(sink, "sink");
        if (sp) { gst_pad_add_probe(sp, GST_PAD_PROBE_TYPE_BUFFER, byte_probe, NULL, NULL);
                  gst_object_unref(sp); }
        gst_object_unref(sink);
    }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(pipeline);
    int rc = 0;
    while (!atomic_load_explicit(&g_repro_stop, memory_order_acquire)) {
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, 200 * GST_MSECOND,
                                                     GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
        if (!msg) continue;
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError *e = NULL; gst_message_parse_error(msg, &e, NULL);
            fprintf(stderr, "video: %s\n", e ? e->message : "error");
            if (e) g_error_free(e);
            rc = -1;
        }
        gst_message_unref(msg);
        break;
    }
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return rc;
}

extern int tts_run(const cam_profile_t *p, const char *ts_pipeline_prefix);  /* tts_wrap.c */

static const char *env_or(const char *k, const char *def)
{
    const char *v = getenv(k);
    return (v && *v) ? v : def;
}

/* nearest standard framerate as a GStreamer fraction "num/den" (MPEG-2 only
 * accepts standard rates; H.264 is tolerant but this is fine for both). */
static void fps_fraction(double fps, int *num, int *den)
{
    struct { double f; int n, d; } tbl[] = {
        {23.976, 24000, 1001}, {24, 24, 1}, {25, 25, 1},
        {29.97, 30000, 1001}, {30, 30, 1}, {50, 50, 1},
        {59.94, 60000, 1001}, {60, 60, 1},
    };
    if (fps <= 0) { *num = 30; *den = 1; return; }
    int best = 0; double bd = 1e9;
    for (int i = 0; i < (int)(sizeof tbl / sizeof tbl[0]); i++) {
        double d = fps > tbl[i].f ? fps - tbl[i].f : tbl[i].f - fps;
        if (d < bd) { bd = d; best = i; }
    }
    if (bd < 0.2) { *num = tbl[best].n; *den = tbl[best].d; }
    else { *num = (int)(fps * 1000 + 0.5); *den = 1000; }
}

/* the raw-video source + caps (camera or test pattern), profile-sized */
static int src_segment(const cam_profile_t *p, char *out, unsigned n)
{
#if defined(__linux__)
    const char *src = env_or("CAMEMU_SRC", "libcamerasrc");
#else
    const char *src = env_or("CAMEMU_SRC", "videotestsrc is-live=true");
#endif
    int w = p->width > 0 ? p->width : 1280;
    int h = p->height > 0 ? p->height : 720;
    int fnum, fden;
    fps_fraction(p->fps, &fnum, &fden);
    return snprintf(out, n,
        "%s ! video/x-raw,width=%d,height=%d,framerate=%d/%d ! videoconvert",
        src, w, h, fnum, fden);
}

/* H.264 encoder segment, profile bitrate/gop/profile-level */
static int h264_enc(const cam_profile_t *p, char *out, unsigned n)
{
#if defined(__linux__)
    const char *enc = env_or("CAMEMU_ENC", "v4l2");
#else
    const char *enc = env_or("CAMEMU_ENC", "x264");
#endif
    long kbps = p->bitrate_kbps > 0 ? p->bitrate_kbps : 4000;
    int gop = p->gop > 0 ? p->gop : 30;
    if (!strcmp(enc, "v4l2"))
        return snprintf(out, n,
            "v4l2h264enc extra-controls=\"controls,video_bitrate=%ld,h264_i_frame_period=%d\" "
            "! video/x-h264,level=(string)4 ! h264parse config-interval=1",
            kbps * 1000, gop);
    return snprintf(out, n,
        "x264enc bitrate=%ld key-int-max=%d speed-preset=veryfast tune=zerolatency "
        "! h264parse config-interval=1", kbps, gop);
}

int video_pipe_build(const cam_profile_t *p, char *out, unsigned n)
{
    char src[256], enc[256];
    src_segment(p, src, sizeof src);

    const char *host = p->dst_ip[0] ? p->dst_ip : "239.1.1.1";
    int port = p->dst_port > 0 ? p->dst_port : 5004;
    int pt = p->payload_type >= 0 ? p->payload_type : 33;
    unsigned ssrc = p->ssrc ? p->ssrc : 0x1234;

    int is_h264 = !strcmp(p->codec, "H.264");
    int is_jpeg = !strcmp(p->codec, "JPEG") || !strcmp(p->container, "rtp-jpeg");
    int is_mp2v = !strcmp(p->codec, "MPEG-2");

    if (is_jpeg) {                       /* Motion-JPEG over RTP (PT26) */
        return snprintf(out, n,
            "%s ! jpegenc ! rtpjpegpay pt=%d ssrc=%u "
            "! udpsink name=sink host=%s port=%d auto-multicast=true",
            src, pt, ssrc, host, port) < (int)n ? 0 : -1;
    }

    /* video encoder -> H.264 or MPEG-2 elementary stream */
    if (is_h264) h264_enc(p, enc, sizeof enc);
    else if (is_mp2v) {
        long kbps = p->bitrate_kbps > 0 ? p->bitrate_kbps : 4000;
        int gop = p->gop > 0 ? p->gop : 15;
        /* Pi HW encoder is H.264-only; MPEG-2 falls back to software (gst-libav).
         * avenc_mpeg2video needs an explicit I420 raw caps. */
        snprintf(enc, sizeof enc,
                 "video/x-raw,format=I420 ! avenc_mpeg2video bitrate=%ld gop-size=%d ! mpegvideoparse",
                 kbps * 1000, gop);
    } else {
        return -1;                       /* unsupported codec */
    }

    /* container + RTP + multicast sink */
    if (!strcmp(p->container, "mpeg2-tts")) {
        /* TTS(192) is handled by tts_wrap: emit TS to an appsink there.
         * video_pipe_run() special-cases this; the string is the TS prefix. */
        return snprintf(out, n, "%s ! %s ! mpegtsmux alignment=7", src, enc) < (int)n ? 0 : -1;
    }
    if (!strcmp(p->container, "mpeg2-ts")) {
        return snprintf(out, n,
            "%s ! %s ! mpegtsmux alignment=7 ! rtpmp2tpay pt=%d ssrc=%u "
            "! udpsink name=sink host=%s port=%d auto-multicast=true",
            src, enc, pt, ssrc, host, port) < (int)n ? 0 : -1;
    }
    if (!strcmp(p->container, "mpeg2-es") || !strcmp(p->container, "raw-es")) {
        /* raw ES over RTP: H.264 via rtph264pay; MPEG-2 ES via rtpmpvpay */
        if (is_h264)
            return snprintf(out, n,
                "%s ! %s ! rtph264pay pt=%d ssrc=%u ! udpsink name=sink host=%s port=%d auto-multicast=true",
                src, enc, pt, ssrc, host, port) < (int)n ? 0 : -1;
        return snprintf(out, n,
            "%s ! %s ! rtpmpvpay pt=%d ssrc=%u ! udpsink name=sink host=%s port=%d auto-multicast=true",
            src, enc, pt, ssrc, host, port) < (int)n ? 0 : -1;
    }
    return -1;
}

int video_pipe_run(const cam_profile_t *p)
{
    gst_init(NULL, NULL);
    char pipe[2048];
    if (video_pipe_build(p, pipe, sizeof pipe) != 0) {
        fprintf(stderr, "video: unsupported container/codec (%s / %s)\n", p->container, p->codec);
        return -1;
    }
    if (getenv("CAMEMU_DUMP")) { printf("%s\n", pipe); return 0; }

    /* MLIT TTS(192): the built string is only the TS-producing prefix; the
     * tts_wrap sender taps it via appsink, wraps 188->192, RTP-payloads PT103
     * and multicasts. */
    if (!strcmp(p->container, "mpeg2-tts"))
        return tts_run(p, pipe);

    GError *err = NULL;
    GstElement *pipeline = gst_parse_launch(pipe, &err);
    if (!pipeline || err) {
        fprintf(stderr, "video: pipeline error: %s\n", err ? err->message : "?");
        if (err) g_error_free(err);
        return -1;
    }
    fprintf(stderr, "video: streaming %s %dx%d to %s:%d (PT=%d)\n",
            p->codec, p->width, p->height, p->dst_ip, p->dst_port, p->payload_type);
    return video_pipe_spin(pipeline);
}
