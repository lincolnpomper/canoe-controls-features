#define _POSIX_C_SOURCE 200809L
#include "../src/frame_pacer.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* Exercise the real clock/sleep with zero emulation cost to prove the ceiling
 * even when a game would otherwise be able to run much faster than 120 fps. */
int main(void) {
    uint64_t last = 0, first = 0;
    for (int i = 0; i <= 360; ++i) {
        if (frame_pacer_wait(&last)) return 1;
        if (!i) first = last;
    }
    uint64_t elapsed = last - first;
    double fps = 360.0 * 1000000000.0 / (double)elapsed;
    printf("intervals=360 elapsed_ns=%llu max_fps=120 measured_fps=%.3f\n",
        (unsigned long long)elapsed, fps);
    if (elapsed < UINT64_C(360) * CANOE_FAST_FRAME_NS) return 2;
    puts("PASS: real-clock ceiling holds with no game workload");
    return 0;
}
