/* h264_inject.c — see h264_inject.h. */
#include "h264_inject.h"
#include <gst/gst.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    unsigned char sps[128]; int sps_len;
    unsigned char pps[64];  int pps_len;
} inj_ctx;

static int b64dec(const char *in, unsigned char *out, int cap)
{
    int o = 0, bits = 0; unsigned acc = 0;
    for (const char *p = in; *p; p++) {
        int c = (unsigned char)*p, v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else continue;
        acc = (acc << 6) | (unsigned)v; bits += 6;
        if (bits >= 8) { bits -= 8; if (o < cap) out[o++] = (unsigned char)((acc >> bits) & 0xFF);
                         acc &= (1u << bits) - 1u; }
    }
    return o;
}

/* start code length at p (3 or 4), or 0 */
static int sc_len(const unsigned char *p, size_t n)
{
    if (n >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) return 4;
    if (n >= 3 && p[0] == 0 && p[1] == 0 && p[2] == 1) return 3;
    return 0;
}

/* rebuild the byte-stream buffer, replacing SPS(7)/PPS(8) NALs with the
 * captured ones (keeping each NAL's original start code length). */
static GstPadProbeReturn on_buf(GstPad *pad, GstPadProbeInfo *info, gpointer u)
{
    (void)pad;
    inj_ctx *c = (inj_ctx *)u;
    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
    GstMapInfo m;
    if (!buf || !gst_buffer_map(buf, &m, GST_MAP_READ)) return GST_PAD_PROBE_OK;

    GByteArray *out = g_byte_array_sized_new((guint)m.size + 128);
    int replaced = 0;
    size_t i = 0;
    while (i < m.size) {
        int scl = sc_len(m.data + i, m.size - i);
        if (!scl) { g_byte_array_append(out, m.data + i, 1); i++; continue; }
        /* find the next start code = end of this NAL */
        size_t j = i + scl;
        while (j < m.size && !sc_len(m.data + j, m.size - j)) j++;
        int type = (m.data[i + scl]) & 0x1F;
        g_byte_array_append(out, m.data + i, (guint)scl);        /* keep start code */
        if (type == 7 && c->sps_len) {
            g_byte_array_append(out, c->sps, (guint)c->sps_len); replaced = 1;
        } else if (type == 8 && c->pps_len) {
            g_byte_array_append(out, c->pps, (guint)c->pps_len); replaced = 1;
        } else {
            g_byte_array_append(out, m.data + i + scl, (guint)(j - i - scl));
        }
        i = j;
    }
    gst_buffer_unmap(buf, &m);

    if (replaced) {
        GstBuffer *nb = gst_buffer_new_allocate(NULL, out->len, NULL);
        gst_buffer_fill(nb, 0, out->data, out->len);
        gst_buffer_copy_into(nb, buf, GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_FLAGS, 0, (gsize)-1);
        gst_buffer_unref(buf);
        GST_PAD_PROBE_INFO_DATA(info) = nb;
    }
    g_byte_array_free(out, TRUE);
    return GST_PAD_PROBE_OK;
}

void h264_inject_attach(void *pipeline_v, const cam_profile_t *p)
{
    if (!p->sps_b64[0]) return;
    static inj_ctx c;
    c.sps_len = b64dec(p->sps_b64, c.sps, sizeof c.sps);
    c.pps_len = p->pps_b64[0] ? b64dec(p->pps_b64, c.pps, sizeof c.pps) : 0;
    if (c.sps_len < 4) return;

    GstElement *pl = (GstElement *)pipeline_v;
    GstElement *hp = gst_bin_get_by_name(GST_BIN(pl), "hparse");
    if (!hp) { fprintf(stderr, "h264-inject: no 'hparse' element\n"); return; }
    GstPad *sp = gst_element_get_static_pad(hp, "src");
    if (sp) {
        gst_pad_add_probe(sp, GST_PAD_PROBE_TYPE_BUFFER, on_buf, &c, NULL);
        gst_object_unref(sp);
        fprintf(stderr, "h264-inject: replacing SPS(%dB)/PPS(%dB) with captured parameter sets\n",
                c.sps_len, c.pps_len);
    }
    gst_object_unref(hp);
}
