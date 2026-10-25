#define _POSIX_C_SOURCE 200809L
#include "frame_pacer.h"
#include <errno.h>
#include <time.h>

static int monotonic_ns(uint64_t *out) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        return -1;
    *out = (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
    return 0;
}

int frame_pacer_wait(uint64_t *previous) {
    uint64_t now;
    if (monotonic_ns(&now))
        goto failed;
    if (*previous) {
        uint64_t target = *previous + CANOE_FAST_FRAME_NS;
        while (now < target) {
            uint64_t remaining = target - now;
            struct timespec delay = {
                .tv_sec = (time_t)(remaining / UINT64_C(1000000000)),
                .tv_nsec = (long)(remaining % UINT64_C(1000000000))
            };
            if (nanosleep(&delay, NULL) && errno != EINTR)
                goto failed;
            if (monotonic_ns(&now))
                goto failed;
        }
    }
    /* A late frame starts a fresh interval. Never accumulate catch-up frames. */
    *previous = now;
    return 0;
failed:
    *previous = 0;
    return -1;
}
