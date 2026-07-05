/* tts_wrap.c — MLIT MPEG2-TTS(192) forward path.
 *
 * GStreamer produces 188-byte MPEG-TS; the MLIT profile wants 192-byte TTS
 * units (4-byte 27 MHz timecode + 188 TS) carried 6-per-RTP-packet at PT=103.
 * This taps the TS via an appsink, wraps each 188 -> 192, groups 6 into a 1152 B
 * RTP payload with a hand-built 12-byte RTP header, and multicasts it.
 *
 * streamkit's sk_tts_to_ts is the reverse (parse) direction; the forward wrap
 * and the RTP payloading loop don't exist there, so they live here. */
#include "profile.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define TTS_UNIT   192
#define TS_PKT     188
#define TTS_PER_RTP 6

typedef struct {
    int      sock;
    struct sockaddr_in dst;
    unsigned ssrc;
    int      pt;
    uint16_t seq;
    uint32_t rtp_ts;             /* 90 kHz */
    uint32_t tc27;               /* 27 MHz TTS timecode, monotonic */
    uint8_t  pending[TTS_UNIT * TTS_PER_RTP];
    int      npend;              /* TTS units buffered (0..6) */
} tts_ctx;

static void rtp_send(tts_ctx *c)
{
    if (c->npend == 0) return;
    uint8_t pkt[12 + TTS_UNIT * TTS_PER_RTP];
    int marker = 1;                                  /* one video frame chunk */
    pkt[0] = 0x80;
    pkt[1] = (uint8_t)((marker << 7) | (c->pt & 0x7F));
    pkt[2] = c->seq >> 8; pkt[3] = c->seq & 0xFF;
    pkt[4] = c->rtp_ts >> 24; pkt[5] = c->rtp_ts >> 16; pkt[6] = c->rtp_ts >> 8; pkt[7] = c->rtp_ts;
    pkt[8] = c->ssrc >> 24; pkt[9] = c->ssrc >> 16; pkt[10] = c->ssrc >> 8; pkt[11] = c->ssrc;
    int plen = c->npend * TTS_UNIT;
    memcpy(pkt + 12, c->pending, plen);
    sendto(c->sock, pkt, 12 + plen, 0, (struct sockaddr *)&c->dst, sizeof c->dst);
    c->seq++;
    c->rtp_ts += 3003;                               /* 90kHz / 29.97 */
    c->npend = 0;
}

/* wrap one 188-byte TS packet into a 192-byte TTS unit into the pending buffer */
static void push_ts(tts_ctx *c, const uint8_t *ts188)
{
    uint8_t *u = c->pending + c->npend * TTS_UNIT;
    u[0] = c->tc27 >> 24; u[1] = c->tc27 >> 16; u[2] = c->tc27 >> 8; u[3] = c->tc27;
    memcpy(u + 4, ts188, TS_PKT);
    c->tc27 += 300;                                  /* 27MHz tick per unit (nominal) */
    c->npend++;
    if (c->npend == TTS_PER_RTP) rtp_send(c);
}

static GstFlowReturn on_sample(GstAppSink *sink, gpointer user)
{
    tts_ctx *c = (tts_ctx *)user;
    GstSample *s = gst_app_sink_pull_sample(sink);
    if (!s) return GST_FLOW_OK;
    GstBuffer *b = gst_sample_get_buffer(s);
    GstMapInfo m;
    if (gst_buffer_map(b, &m, GST_MAP_READ)) {
        for (gsize off = 0; off + TS_PKT <= m.size; off += TS_PKT) {
            if (m.data[off] != 0x47) continue;       /* TS sync */
            push_ts(c, m.data + off);
        }
        gst_buffer_unmap(b, &m);
    }
    gst_sample_unref(s);
    return GST_FLOW_OK;
}

int tts_run(const cam_profile_t *p, const char *ts_prefix)
{
    static tts_ctx c;
    memset(&c, 0, sizeof c);
    c.ssrc = p->ssrc ? p->ssrc : 0x1234;
    c.pt = p->payload_type >= 0 ? p->payload_type : 103;

    c.sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (c.sock < 0) { perror("socket"); return -1; }
    unsigned char ttl = 16;
    setsockopt(c.sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
    memset(&c.dst, 0, sizeof c.dst);
    c.dst.sin_family = AF_INET;
    c.dst.sin_port = htons(p->dst_port > 0 ? p->dst_port : 5004);
    inet_pton(AF_INET, p->dst_ip[0] ? p->dst_ip : "239.1.1.1", &c.dst.sin_addr);

    char pipe[2048];
    snprintf(pipe, sizeof pipe, "%s ! appsink name=out sync=false", ts_prefix);
    GError *err = NULL;
    GstElement *pl = gst_parse_launch(pipe, &err);
    if (!pl || err) { fprintf(stderr, "tts: pipeline: %s\n", err ? err->message : "?");
                      if (err) g_error_free(err); return -1; }
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pl), "out");
    GstAppSinkCallbacks cb = { 0 };
    cb.new_sample = on_sample;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &cb, &c, NULL);

    gst_element_set_state(pl, GST_STATE_PLAYING);
    fprintf(stderr, "video: MLIT TTS(192) PT=%d ssrc=0x%08x -> %s:%d\n",
            c.pt, c.ssrc, p->dst_ip, p->dst_port);

    GstBus *bus = gst_element_get_bus(pl);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                 GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    if (msg) gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pl, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pl);
    close(c.sock);
    return 0;
}
