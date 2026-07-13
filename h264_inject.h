/* h264_inject.h — byte-exact SPS/PPS injection.
 *
 * The encoder produces its own SPS/PPS. For byte-exact reproduction we replace
 * them with the exact parameter sets captured in the profile (sps_b64/pps_b64):
 * a pad probe on the H.264 byte-stream rewrites every SPS(7)/PPS(8) NAL with the
 * captured bytes. Only used when the profile carries them; if the resolution/
 * profile of the captured SPS matches the encoder output (it does by
 * construction) the stream still decodes, but now with the original headers. */
#ifndef CAMEMU_H264_INJECT_H
#define CAMEMU_H264_INJECT_H

#include "profile.h"

/* Attach the SPS/PPS replacer to the element named "hparse" in `pipeline`
 * (a GstElement*). No-op if the profile has no captured SPS. */
void h264_inject_attach(void *pipeline, const cam_profile_t *p);

#endif /* CAMEMU_H264_INJECT_H */
