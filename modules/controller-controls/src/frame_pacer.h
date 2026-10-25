#ifndef FRAME_PACER_H
#define FRAME_PACER_H
#include <stdint.h>

/* Ceil(1 second / 120). No faster than 2x the verified 60 Hz game loop. */
#define CANOE_FAST_FRAME_NS UINT64_C(8333334)

/* previous is the actual completion time, not a debt to catch up later.
 * Set it to zero on release/pause. Failure means use native pacing instead. */
int frame_pacer_wait(uint64_t *previous);
#endif
