#define _GNU_SOURCE

#include "canoe_pause.h"
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/* Exact fixed-address ARM ELF profile, verified on an SNES Classic Mini.
 * Installer SHA-256: 30d0768623e875dc8f3de334ccc7193807878c8ad9b50c38bec362deb4245a61.
 * Runtime checks the executable identity, mapped segments and both routines
 * before reading any private state. Other builds retain analog input only. */
#define PAUSE_ENTRY ((void (*)(int))(uintptr_t)0x0002cac4u)
#define PAUSED (*(volatile uint8_t *)(uintptr_t)0x001e210cu)
#define AUDIO_PAUSED (*(volatile uint8_t *)(uintptr_t)0x001dffc4u)
#define FRAME (*(volatile uint32_t *)(uintptr_t)0x001e2100u)
#define RUN_STATE (*(volatile uint32_t *)(uintptr_t)0x001e212cu)
#define ACTIVE (*(volatile uint32_t *)(uintptr_t)0x001e2130u)
#define EXPECTED_OBJECT 0x00223688u
#define EXPECTED_VTABLE 0x001b87f8u
#define AUDIO_PAUSE_ENTRY 0x000e3f04u

static int supported;
static int log_fd = -1;
static size_t log_bytes;

static void record(const char *event, int32_t instance) {
    if (log_fd < 0 || log_bytes >= 65536)
        return;
    struct timespec t = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &t);
    char line[320];
    int n = snprintf(line, sizeof(line),
        "ms=%llu pid=%ld tid=%ld event=%s instance=%d supported=%d"
        " state=%u paused=%u audio_paused=%u frame=%u\n",
        (unsigned long long)t.tv_sec * 1000 + (unsigned long long)t.tv_nsec / 1000000,
        (long)getpid(), (long)syscall(SYS_gettid), event, (int)instance, supported,
        supported ? RUN_STATE : 0, supported ? (unsigned)PAUSED : 0,
        supported ? (unsigned)AUDIO_PAUSED : 0, supported ? FRAME : 0);
    if (n > 0) {
        size_t count = (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1;
        ssize_t written = write(log_fd, line, count);
        if (written > 0)
            log_bytes += (size_t)written;
    }
}

int canoe_native_supported(void) { return supported; }

int canoe_game_ready(void) {
    return supported && RUN_STATE == 3 && ACTIVE == EXPECTED_OBJECT &&
        *(volatile uint32_t *)(uintptr_t)EXPECTED_OBJECT == EXPECTED_VTABLE &&
        ((volatile uint32_t *)(uintptr_t)EXPECTED_VTABLE)[17] == AUDIO_PAUSE_ENTRY &&
        PAUSED <= 1 && AUDIO_PAUSED <= 1;
}

void canoe_controls_record(const char *event, int32_t instance_id) {
    record(event, instance_id);
}

static int check_segments(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    if (info->dlpi_name && info->dlpi_name[0])
        return 0;
    if (info->dlpi_addr != 0 || info->dlpi_phnum != 8)
        return 1;
    unsigned loads = 0;
    int code = 0, state = 0;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *p = &info->dlpi_phdr[i];
        if (p->p_type != PT_LOAD)
            continue;
        ++loads;
        if (p->p_vaddr == 0x10000 && p->p_offset == 0 &&
            p->p_filesz == 1795612 && p->p_memsz == 1795612 &&
            p->p_flags == (PF_R | PF_X))
            code = 1;
        if (p->p_vaddr == 1926684 && p->p_offset == 1795612 &&
            p->p_filesz == 39028 && p->p_memsz == 5251780 &&
            p->p_flags == (PF_R | PF_W))
            state = 1;
    }
    *(int *)data = loads == 2 && code && state;
    return 1;
}

__attribute__((constructor)) static void pause_start(void) {
    char exe[512];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n < 0)
        return;
    exe[n] = 0;
    const char *base = strrchr(exe, '/');
    if (!base || strcmp(base + 1, "canoe-shvc"))
        return;
    log_fd = open("/tmp/canoe-controls.log", O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, 0644);
    struct stat st;
    if (log_fd >= 0 && !fstat(log_fd, &st) && st.st_size > 0)
        log_bytes = (size_t)st.st_size;
    int mapped = 0;
    if (!stat("/proc/self/exe", &st) && st.st_size == 1836012)
        dl_iterate_phdr(check_segments, &mapped);
    const uint32_t pause_signature[] = {
        0xe30230b4, 0xe1a01000, 0xe340301e, 0xe593207c, 0xe5c30058,
        0xe592c000, 0xe1a00002, 0xe59c3044, 0xe12fff13
    };
    const uint32_t audio_signature[] = {
        0xe2911000, 0xe30f3fac, 0xe340301d, 0x13a01001, 0xe5c31018, 0xe12fff1e
    };
    if (mapped &&
        !memcmp((const void *)(uintptr_t)0x2cac4, pause_signature, sizeof(pause_signature)) &&
        !memcmp((const void *)(uintptr_t)AUDIO_PAUSE_ENTRY, audio_signature, sizeof(audio_signature)))
        supported = 1;
    record(supported ? "pause_ready" : "pause_unsupported_binary", -1);
}

void canoe_pause_toggle(int32_t instance_id) {
    if (!supported)
        return;
    /* State 3 is the normal game loop. The exact object/vtable were checked
     * during the earlier pause observer tests; never call through a stale one. */
    if (!canoe_game_ready()) {
        record("zl_refused_state", instance_id);
        return;
    }
    /* Read Canoe's real flag so F5 or another native action cannot desync us.
     * This call runs on the SDL polling thread and also pauses/resumes audio. */
    int next = !PAUSED;
    PAUSE_ENTRY(next);
    record(next ? "zl_paused" : "zl_resumed", instance_id);
}

__attribute__((destructor)) static void pause_stop(void) {
    if (log_fd >= 0) {
        record("controls_unloaded", -1);
        close(log_fd);
    }
}
