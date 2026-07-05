/* rtsp_serve.h — serve the reproduced camera as an RTSP server.
 *
 * When the profile's control protocol is RTSP, camemu behaves like an RTSP
 * camera: it answers DESCRIBE/SETUP/PLAY and streams the encoded video (unicast
 * or multicast per the client's / profile's transport), instead of blindly
 * multicasting. Uses gst-rtsp-server (GstRTSPMediaFactory launch pipeline). */
#ifndef CAMEMU_RTSP_SERVE_H
#define CAMEMU_RTSP_SERVE_H

#include "profile.h"

/* Run an RTSP server for `p` (blocks on a GMainLoop). 0 ok, -1 error.
 * URL path from profile url (default /stream1), port from server_port (554),
 * payload from the profile codec/PT. */
int rtsp_serve_run(const cam_profile_t *p);

#endif /* CAMEMU_RTSP_SERVE_H */
