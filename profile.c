/* profile.c — see profile.h. A tiny scoped extractor for the fixed PacketScope
 * schema (not a general JSON parser): it locates the "control", "video" and
 * "codec" object regions, then pulls known keys from the right region so
 * duplicate keys (e.g. "transport" in both control and video) don't collide. */
#include "profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* value of "key" within [s, end): copies a JSON string into out (unescaped
 * minimally). Returns 1 if found. */
static int jstr(const char *s, const char *end, const char *key, char *out, size_t n)
{
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p || p >= end) { if (out && n) out[0] = 0; return 0; }
    p += strlen(pat);
    while (p < end && (*p == ' ' || *p == ':')) p++;
    if (p >= end || *p != '"') return 0;
    p++;
    size_t w = 0;
    while (p < end && *p != '"' && w + 1 < n) {
        if (*p == '\\' && p + 1 < end) {
            p++;                         /* JSON escape: translate common ones */
            char c = *p++;
            out[w++] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
        } else {
            out[w++] = *p++;
        }
    }
    out[w] = 0;
    return 1;
}

/* numeric value of "key" within [s,end). def if absent. */
static long jnum(const char *s, const char *end, const char *key, long def)
{
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p || p >= end) return def;
    p += strlen(pat);
    while (p < end && (*p == ' ' || *p == ':')) p++;
    if (p >= end) return def;
    if (*p == '"') p++;                 /* some numbers may be quoted */
    return strtol(p, NULL, 0);
}

static double jflt(const char *s, const char *end, const char *key, double def)
{
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p || p >= end) return def;
    p += strlen(pat);
    while (p < end && (*p == ' ' || *p == ':')) p++;
    if (p >= end) return def;
    return strtod(p, NULL);
}

static int jbool(const char *s, const char *end, const char *key, int def)
{
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p || p >= end) return def;
    p += strlen(pat);
    while (p < end && (*p == ' ' || *p == ':')) p++;
    if (p + 4 <= end && !strncmp(p, "true", 4)) return 1;
    if (p + 5 <= end && !strncmp(p, "false", 5)) return 0;
    return def;
}

int cam_profile_parse(const char *json, cam_profile_t *out)
{
    memset(out, 0, sizeof *out);
    out->media_port = out->server_port = out->video_pid = out->dst_port = -1;
    out->payload_type = -1; out->cctv_profile = -1;
    if (!json) return -1;

    const char *docend = json + strlen(json);
    const char *ctrl = strstr(json, "\"control\"");
    const char *vid  = strstr(json, "\"video\"");
    if (!vid) return -1;                /* video block is mandatory */
    const char *codec = strstr(vid, "\"codec\"");

    const char *ctrl_end = vid ? vid : docend;      /* control region ends where video starts */
    const char *codec_end = docend;
    const char *vid_end = codec ? codec : docend;   /* video top-level keys before codec block */

    /* control */
    if (ctrl) {
        jstr(ctrl, ctrl_end, "protocol", out->ctrl_proto, sizeof out->ctrl_proto);
        jstr(ctrl, ctrl_end, "url", out->url, sizeof out->url);
        jstr(ctrl, ctrl_end, "transport", out->transport, sizeof out->transport);
        jstr(ctrl, ctrl_end, "multicast_group", out->mcast_group, sizeof out->mcast_group);
        out->media_port = (int)jnum(ctrl, ctrl_end, "media_port", -1);
        out->server_port = (int)jnum(ctrl, ctrl_end, "server_port", -1);
    }
    /* video (top-level, before codec) */
    jstr(vid, vid_end, "container", out->container, sizeof out->container);
    out->payload_type = (int)jnum(vid, vid_end, "payload_type", -1);
    { char ss[16]; if (jstr(vid, vid_end, "ssrc", ss, sizeof ss)) out->ssrc = (unsigned)strtoul(ss, NULL, 0); }
    out->clock_hz = (int)jnum(vid, vid_end, "clock_hz", 90000);
    out->tts_unit = (int)jnum(vid, vid_end, "tts_unit", 0);
    out->video_pid = (int)jnum(vid, vid_end, "video_pid", -1);
    out->bitrate_kbps = jnum(vid, docend, "bitrate_kbps", 0);
    out->cctv_profile = jbool(vid, docend, "cctv_profile_conformant", -1);
    jstr(vid, docend, "src_ip", out->src_ip, sizeof out->src_ip);
    jstr(vid, docend, "dst_ip", out->dst_ip, sizeof out->dst_ip);
    out->dst_port = (int)jnum(vid, docend, "dst_port", -1);
    /* codec block */
    if (codec) {
        jstr(codec, codec_end, "name", out->codec, sizeof out->codec);
        jstr(codec, codec_end, "profile", out->h264_profile, sizeof out->h264_profile);
        { char lv[16]; if (jstr(codec, codec_end, "level", lv, sizeof lv)) {
              int a = 0, b = 0; sscanf(lv, "%d.%d", &a, &b); out->level_x10 = a * 10 + b; } }
        out->width = (int)jnum(codec, codec_end, "width", 0);
        out->height = (int)jnum(codec, codec_end, "height", 0);
        out->fps = jflt(codec, codec_end, "fps", 0);
        out->gop = (int)jnum(codec, codec_end, "gop", 0);
        out->aud = jbool(codec, codec_end, "aud", -1);
        out->sei = jbool(codec, codec_end, "sei", -1);
        out->inband_sps = jbool(codec, codec_end, "inband_sps", -1);
        jstr(codec, codec_end, "sps", out->sps_b64, sizeof out->sps_b64);
        jstr(codec, codec_end, "pps", out->pps_b64, sizeof out->pps_b64);
    } else {
        out->aud = out->sei = out->inband_sps = -1;
    }
    if (ctrl) jstr(ctrl, ctrl_end, "sdp", out->sdp, sizeof out->sdp);
    return 0;
}

int cam_profile_load(const char *path, cam_profile_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return cam_profile_parse(buf, out);
}

void cam_profile_dump(const cam_profile_t *p)
{
    printf("control: proto=%s url=%s group=%s media_port=%d server_port=%d\n",
           p->ctrl_proto, p->url, p->mcast_group, p->media_port, p->server_port);
    printf("  transport=%s\n", p->transport);
    printf("video: container=%s pt=%d ssrc=0x%08x clock=%d tts_unit=%d pid=%d\n",
           p->container, p->payload_type, p->ssrc, p->clock_hz, p->tts_unit, p->video_pid);
    printf("codec: %s %s L%d.%d %dx%d %.2ffps gop=%d %ldkbps aud=%d sei=%d inband=%d\n",
           p->codec, p->h264_profile, p->level_x10 / 10, p->level_x10 % 10,
           p->width, p->height, p->fps, p->gop, p->bitrate_kbps,
           p->aud, p->sei, p->inband_sps);
    if (p->sps_b64[0]) printf("  sps(b64)=%s\n  pps(b64)=%s\n", p->sps_b64, p->pps_b64);
    if (p->sdp[0])     printf("  sdp=%s\n", p->sdp);
    printf("dest: %s -> %s:%d  cctv_conformant=%d\n",
           p->src_ip, p->dst_ip, p->dst_port, p->cctv_profile);
}
