/* mjpeg_serve.h — serve the reproduced camera as HTTP Motion-JPEG.
 *
 * Classic IP-camera MJPEG: an HTTP endpoint that answers GET with a
 * "multipart/x-mixed-replace" stream of JPEG frames. Used when the profile's
 * control protocol is http-mjpeg (or codec JPEG without RTP). Encodes the
 * built-in camera to JPEG (GStreamer) and pushes each frame as a multipart
 * part; also answers the HTTP GET request (the "request" side). */
#ifndef CAMEMU_MJPEG_SERVE_H
#define CAMEMU_MJPEG_SERVE_H

#include "profile.h"

/* Run the HTTP-MJPEG server for `p` (blocks). Port from server_port (default
 * 80), path from the control URL. 0 ok, -1 error. */
int mjpeg_serve_run(const cam_profile_t *p);

#endif /* CAMEMU_MJPEG_SERVE_H */
