/* repro.h — start/stop the stream reproduction on a background thread and
 * expose live stats, so a UI can drive it without blocking. The CLI keeps
 * using the blocking *_run() entry points directly; the UI uses this. */
#ifndef CAMSTREAMEMU_REPRO_H
#define CAMSTREAMEMU_REPRO_H

#include "profile.h"
#include <stdatomic.h>
#include <stdint.h>

/* shared with the delivery modules so each can bump the byte/packet counters */
extern _Atomic uint64_t g_repro_bytes;
extern _Atomic uint32_t g_repro_packets;
extern _Atomic int      g_repro_stop;     /* the *_run loops poll this to exit */

typedef struct { int running; uint64_t bytes; uint32_t packets; uint32_t elapsed_s; char dest[48]; char mode[12]; } repro_stats;

/* Start reproducing `p` (copied). mode: 0 auto (per profile), 1 rtsp, 2 mjpeg,
 * 3 multicast. 0 ok, -1 error. Non-blocking. */
int  repro_start(const cam_profile_t *p, int mode);
void repro_stop(void);
void repro_get(repro_stats *out);
int  repro_running(void);

#endif /* CAMSTREAMEMU_REPRO_H */
