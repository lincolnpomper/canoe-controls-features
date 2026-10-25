#define _GNU_SOURCE
#include "canoe_fast_forward.h"
#include "canoe_pause.h"
#include "frame_pacer.h"
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static int (*real_ioctl)(int, unsigned long, void *);
static int requested;
static int checked, compatible;
static uint64_t previous_frame_ns;

void canoe_fast_forward_set(int held, int32_t instance_id) {
    held = !!held;
    if (__atomic_load_n(&requested, __ATOMIC_RELAXED) == held)
        return;
    __atomic_store_n(&requested, held, __ATOMIC_RELAXED);
    previous_frame_ns = 0;
    canoe_controls_record(held ? "zr_held" : "zr_released", instance_id);
}

static int profile_matches(void) {
    if (!canoe_native_supported())
        return 0;
    if (!checked) {
        /* Exact instructions immediately before the tested framebuffer wait. */
        const uint32_t signature[] = {
            0xe5980000, 0xe28d20c8, 0xe3041620,
            0xe52240a4, 0xe3441004, 0xebfad19a
        };
        compatible = !memcmp((void *)(uintptr_t)0x162698, signature, sizeof(signature));
        checked = 1;
        canoe_controls_record(compatible ? "fast_forward_ready_120fps" : "fast_forward_unsupported_path", -1);
    }
    return compatible;
}

/* ARM EABI uses r0/r1/r2 for ioctl's arguments. A fixed third parameter keeps
 * the caller's r2 intact, even for requests where the kernel ignores it. */
int ioctl(int fd, unsigned long request, void *argument) {
    if (!real_ioctl)
        real_ioctl = (void *)dlsym(RTLD_NEXT, "ioctl");
    if (!real_ioctl) {
        errno = ENOSYS;
        return -1;
    }
    if (request != 0x40044620ul ||
        (uintptr_t)__builtin_return_address(0) != 0x1626b0 ||
        !__atomic_load_n(&requested, __ATOMIC_RELAXED))
        return real_ioctl(fd, request, argument);
    /* Only the measured main-thread CPU renderer is supported. Other threads,
     * requests, games states and pause always use Canoe's original wait. */
    if ((long)syscall(SYS_gettid) != (long)getpid())
        return real_ioctl(fd, request, argument);
    if (!profile_matches() || !canoe_game_ready() ||
        *(volatile uint8_t *)(uintptr_t)0x1e210c ||
        !*(volatile uint8_t *)(uintptr_t)0x223784 ||
        !*(volatile uint8_t *)(uintptr_t)0x22373d) {
        previous_frame_ns = 0;
        return real_ioctl(fd, request, argument);
    }
    int saved_errno = errno;
    if (frame_pacer_wait(&previous_frame_ns))
        return real_ioctl(fd, request, argument);
    errno = saved_errno;
    return 0;
}
