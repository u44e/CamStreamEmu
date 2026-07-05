/* video_pipe.h — GStreamer video-reproduction pipeline from a cam_profile_t.
 *
 * Builds a pipeline that encodes the built-in camera to the profile's codec /
 * resolution / fps / GOP / bitrate, muxes to the profile's container, RTP-
 * payloads with the profile's PT/SSRC, and sends to the profile's multicast
 * destination — reproducing the captured camera's video format.
 *
 * Source/encoder are overridable so it runs on a dev host (videotestsrc +
 * x264enc) as well as a Pi (libcamerasrc + v4l2h264enc):
 *   CAMEMU_SRC   gst source element (default: libcamerasrc, or videotestsrc)
 *   CAMEMU_ENC   h264 encoder: "v4l2" | "x264" (default: v4l2 on linux, x264 else)
 *   CAMEMU_DUMP  if set, print the pipeline string and exit
 */
#ifndef CAMEMU_VIDEO_PIPE_H
#define CAMEMU_VIDEO_PIPE_H

#include "profile.h"

/* Build the gst pipeline string for `p` into `out` (>= 1024). Returns 0 ok,
 * -1 if the container/codec is unsupported. Exposed for testing/--dump. */
int  video_pipe_build(const cam_profile_t *p, char *out, unsigned n);

/* Build + run the pipeline until SIGINT/error/g_repro_stop. 0 ok, -1 error.
 * Blocks. (For mpeg2-tts it wires an appsink tap and the tts_wrap sender.) */
int  video_pipe_run(const cam_profile_t *p);

/* Run an already-built GstElement* pipeline (takes ownership): attaches the
 * byte probe to an element named "sink", plays until ERROR/EOS/g_repro_stop. */
int  video_pipe_spin(void *pipeline);

#endif /* CAMEMU_VIDEO_PIPE_H */
