#define _POSIX_C_SOURCE 200809L
#include "../src/frame_pacer.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

static uint64_t now = UINT64_C(1000000000);
static unsigned sleeps;
static int interrupt_once, sleep_error, clock_error;
int fake_clock_gettime(clockid_t clock, struct timespec *t) {
    assert(clock == CLOCK_MONOTONIC);
    if (clock_error) { errno = EIO; return -1; }
    t->tv_sec = (time_t)(now / UINT64_C(1000000000));
    t->tv_nsec = (long)(now % UINT64_C(1000000000));
    return 0;
}
int fake_nanosleep(const struct timespec *delay, struct timespec *remaining) {
    (void)remaining;
    ++sleeps;
    if (sleep_error) { errno = EIO; return -1; }
    uint64_t ns = (uint64_t)delay->tv_sec * UINT64_C(1000000000) + (uint64_t)delay->tv_nsec;
    if (interrupt_once) {
        interrupt_once = 0;
        now += ns / 2;
        errno = EINTR;
        return -1;
    }
    now += ns;
    return 0;
}
int main(void) {
    uint64_t last = 0;
    assert(!frame_pacer_wait(&last));
    uint64_t start = now;
    for (unsigned i = 0; i < 10000; ++i) {
        uint64_t before = last;
        now += i % 2 ? UINT64_C(3000000) : 0; /* Faster than hardware limit. */
        assert(!frame_pacer_wait(&last));
        assert(last - before >= CANOE_FAST_FRAME_NS);
    }
    assert(now - start >= UINT64_C(10000) * CANOE_FAST_FRAME_NS);
    unsigned before_sleeps = sleeps;
    now += CANOE_FAST_FRAME_NS * 5; /* Slow frame: no added wait or debt. */
    assert(!frame_pacer_wait(&last) && sleeps == before_sleeps);
    uint64_t late = now;
    assert(!frame_pacer_wait(&last));
    assert(now - late >= CANOE_FAST_FRAME_NS); /* No catch-up burst. */
    interrupt_once = 1;
    uint64_t before = last;
    assert(!frame_pacer_wait(&last) && last - before >= CANOE_FAST_FRAME_NS);
    sleep_error = 1;
    assert(frame_pacer_wait(&last) == -1 && last == 0);
    sleep_error = 0;
    assert(!frame_pacer_wait(&last));
    clock_error = 1;
    assert(frame_pacer_wait(&last) == -1 && last == 0);
    clock_error = 0;
    before_sleeps = sleeps;
    now += UINT64_C(5000000000); /* Resume/repress after a reset. */
    assert(!frame_pacer_wait(&last) && sleeps == before_sleeps);
    puts("PASS: 120 fps ceiling over 10000 frames, slow frames, no catch-up bursts, interrupted sleep, clock/sleep failure and reset");
    return 0;
}
