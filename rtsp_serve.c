/* rtsp_serve.c — see rtsp_serve.h. */
#include "rtsp_serve.h"
#include "video_pipe.h"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* the media-factory launch: camera -> encode -> RTP payloader named pay0.
 * gst-rtsp-server appends the RTP session; we only build up to the payloader. */
static int rtsp_launch(const cam_profile_t *p, char *out, unsigned n)
{
    /* reuse video_pipe's encoder selection by asking for an mpeg2-ts pipeline
     * up to the payloader, but RTSP wants pt as the client negotiates; for
     * H.264 we serve elementary H.264 (rtph264pay), which is the common CCTV
     * RTSP case. TS-in-RTP is used when the profile container is mpeg2-ts. */
    char src[256];
    /* borrow the raw-source + encoder builder via a throwaway TS build, then
     * swap the tail; simpler: reconstruct here. */
    (void)src;

    char pipe[2048];
    if (video_pipe_build(p, pipe, sizeof pipe) != 0) return -1;

    /* video_pipe_build for mpeg2-ts ends with 'udpsink ...'; strip the sink and
     * the rtpmp2tpay (RTSP adds its own). For H.264 we want an ES payloader. */
    int is_h264 = !strcmp(p->codec, "H.264");
    long kbps = p->bitrate_kbps > 0 ? p->bitrate_kbps : 4000;
    int gop = p->gop > 0 ? p->gop : 30;
    int w = p->width > 0 ? p->width : 1280, h = p->height > 0 ? p->height : 720;
    int fn = p->fps > 0 ? (int)(p->fps * 1000 + 0.5) : 30000;
#if defined(__linux__)
    const char *src_el = getenv("CAMEMU_SRC") ? getenv("CAMEMU_SRC") : "libcamerasrc";
    const char *enc = getenv("CAMEMU_ENC") ? getenv("CAMEMU_ENC") : "v4l2";
#else
    const char *src_el = getenv("CAMEMU_SRC") ? getenv("CAMEMU_SRC") : "videotestsrc is-live=true";
    const char *enc = getenv("CAMEMU_ENC") ? getenv("CAMEMU_ENC") : "x264";
#endif
    if (!is_h264) return -1;                      /* RTSP path: H.264 for now */

    const char *encseg;
    char encbuf[256];
    if (!strcmp(enc, "v4l2"))
        snprintf(encbuf, sizeof encbuf,
                 "v4l2h264enc extra-controls=\"controls,video_bitrate=%ld,h264_i_frame_period=%d\" ! h264parse config-interval=1",
                 kbps * 1000, gop);
    else
        snprintf(encbuf, sizeof encbuf,
                 "x264enc bitrate=%ld key-int-max=%d speed-preset=veryfast tune=zerolatency ! h264parse config-interval=1",
                 kbps, gop);
    encseg = encbuf;

    int pt = p->payload_type >= 0 ? p->payload_type : 96;
    int wr = snprintf(out, n,
        "( %s ! video/x-raw,width=%d,height=%d,framerate=%d/1000 ! videoconvert ! %s "
        "! rtph264pay name=pay0 pt=%d )",
        src_el, w, h, fn, encseg, pt);
    return (wr > 0 && wr < (int)n) ? 0 : -1;
}

int rtsp_serve_run(const cam_profile_t *p)
{
    gst_init(NULL, NULL);
    char launch[2048];
    if (rtsp_launch(p, launch, sizeof launch) != 0) {
        fprintf(stderr, "rtsp: unsupported codec for RTSP path (%s)\n", p->codec);
        return -1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GstRTSPServer *server = gst_rtsp_server_new();
    int port = p->server_port > 0 ? p->server_port : 554;
    char portstr[8]; snprintf(portstr, sizeof portstr, "%d", port);
    gst_rtsp_server_set_service(server, portstr);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, launch);
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    /* multicast if the profile's transport asked for it */
    if (p->mcast_group[0] || strstr(p->transport, "multicast")) {
        gst_rtsp_media_factory_set_protocols(factory, GST_RTSP_LOWER_TRANS_UDP_MCAST |
                                                      GST_RTSP_LOWER_TRANS_UDP |
                                                      GST_RTSP_LOWER_TRANS_TCP);
        GstRTSPAddressPool *pool = gst_rtsp_address_pool_new();
        if (p->mcast_group[0])
            gst_rtsp_address_pool_add_range(pool, p->mcast_group, p->mcast_group,
                p->dst_port > 0 ? p->dst_port : 5004,
                (p->dst_port > 0 ? p->dst_port : 5004) + 1, 16);
        gst_rtsp_media_factory_set_address_pool(factory, pool);
        g_object_unref(pool);
    }

    /* mount path from the profile URL (e.g. rtsp://host/stream1 -> /stream1) */
    const char *mount = "/stream1";
    if (p->url[0]) {
        const char *s = strstr(p->url, "://");
        s = s ? strchr(s + 3, '/') : NULL;
        if (s && *s) mount = s;
    }
    gst_rtsp_mount_points_add_factory(mounts, mount, factory);
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        fprintf(stderr, "rtsp: cannot bind port %d\n", port);
        return -1;
    }
    fprintf(stderr, "rtsp: serving rtsp://127.0.0.1:%d%s (%s %dx%d)\n",
            port, mount, p->codec, p->width, p->height);
    g_main_loop_run(loop);
    return 0;
}
