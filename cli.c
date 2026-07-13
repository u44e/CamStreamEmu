/* CamStreamEmu — camera stream reproduction (no control) (Linux/Raspberry Pi).
 *
 * Reproduces a real CCTV camera captured by PacketScope: reads the camera
 * profile JSON, encodes the built-in camera to that video format via GStreamer
 * (HW on Pi) and delivers it (multicast / RTSP / HTTP-MJPEG). The MLIT PTZF
 * control plane (mlit_device) is out of scope here; 
 *
 *   camvideo <profile.json>            reproduce the video stream
 *   camvideo --dump <profile.json>     print the parsed profile + gst pipeline
 */
#include "profile.h"
#include "video_pipe.h"
#include "rtsp_serve.h"
#include "mjpeg_serve.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CAMSTREAMEMU_VERSION "0.1.1"

int main(int argc, char **argv)
{
    const char *path = NULL;
    int dump = 0, force_rtsp = 0, force_mcast = 0, force_mjpeg = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dump")) dump = 1;
        else if (!strcmp(argv[i], "--rtsp")) force_rtsp = 1;
        else if (!strcmp(argv[i], "--multicast")) force_mcast = 1;
        else if (!strcmp(argv[i], "--mjpeg")) force_mjpeg = 1;
        else if (!strcmp(argv[i], "--version")) { printf("CamStreamEmu %s\n", CAMSTREAMEMU_VERSION); return 0; }
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "CamStreamEmu %s — reproduce a PacketScope camera profile\n"
                        "usage: camstreamemu [--dump|--rtsp|--mjpeg|--multicast|--version] <camera_profile.json>\n",
                CAMSTREAMEMU_VERSION);
        return 2;
    }

    cam_profile_t p;
    if (cam_profile_load(path, &p) != 0) {
        fprintf(stderr, "cannot load profile: %s\n", path);
        return 1;
    }
    cam_profile_dump(&p);

    if (dump) {
        char pipe[2048];
        if (video_pipe_build(&p, pipe, sizeof pipe) == 0) printf("\npipeline:\n%s\n", pipe);
        else printf("\n(unsupported container/codec for pipeline)\n");
        return 0;
    }

    /* P4: start mlit_device control server here (own thread) before streaming. */

    /* Delivery mode by the camera's control plane:
     *   http-mjpeg -> HTTP multipart MJPEG server
     *   rtsp       -> RTSP server (DESCRIBE/SETUP/PLAY)
     *   else       -> bare multicast push (RTP/TS/TTS/JPEG per container)  */
    if (force_mjpeg || (!force_rtsp && !force_mcast &&
        (!strcmp(p.ctrl_proto, "http-mjpeg") || !strcmp(p.container, "http-mjpeg"))))
        return mjpeg_serve_run(&p) == 0 ? 0 : 1;
    int use_rtsp = force_rtsp || (!force_mcast && !strcmp(p.ctrl_proto, "rtsp"));
    if (use_rtsp)
        return rtsp_serve_run(&p) == 0 ? 0 : 1;
    return video_pipe_run(&p) == 0 ? 0 : 1;
}
