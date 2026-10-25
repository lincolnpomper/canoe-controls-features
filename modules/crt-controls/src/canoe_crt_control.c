#define _GNU_SOURCE
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include "arm_hook.h"

#define WORKER_ENTRY ((void *)0x00163084u)
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
#ifndef SHADOW_DEFAULT
#define SHADOW_DEFAULT 114
#endif
#ifndef SCANLINE_SECOND_DEFAULT
#define SCANLINE_SECOND_DEFAULT 85
#endif
#define SHADOW_MIN 0
#define SHADOW_MAX 199
#define SHADOW_STEP 10
#define KEY_DELAY_MS 400u
#define REPEAT_INTERVAL_MS 100u
#define SETTINGS_SAVE_DELAY_MS 1000u
#define RESET_CHORD_HOLD_MS 1500u
#ifndef SETTINGS_DIR
#define SETTINGS_DIR "/var/lib/hakchi/canoe-filter-worker-probe"
#endif
#ifndef SETTINGS_PATH
#define SETTINGS_PATH SETTINGS_DIR "/settings.conf"
#endif
enum {
    SDL_CONTROLLERAXISMOTION = 0x650,
    SDL_CONTROLLERDEVICEREMOVED = 0x654,
    SDL_RIGHTX = 2,
    SDL_RIGHTY = 3,
    AXIS_ENTER_THRESHOLD = 18000,
    AXIS_EXIT_THRESHOLD = 12000,
    ZL_AXIS = 4,
    ZR_AXIS = 5
};

typedef union {
    uint32_t type;
    struct {
        uint32_t type, timestamp, which;
        uint8_t axis, padding1, padding2, padding3;
        int16_t value;
        uint16_t padding4;
    } caxis;
    struct {
        uint32_t type, timestamp;
        int32_t which;
    } cdevice;
    uint8_t pad[56];
} SDL_Event_Compat;

typedef int (*poll_event_fn)(void *);
#endif

static struct arm_hook worker_hook;
static int log_fd = -1;
static volatile uint32_t logged;
static volatile uint32_t logged_shader_attributes;
typedef void (*gl_vertex_attrib_pointer_fn)(unsigned int, int, unsigned int, unsigned char,
                                             int, const void *);
static gl_vertex_attrib_pointer_fn real_gl_vertex_attrib_pointer;
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
static poll_event_fn real_poll;
static volatile int32_t adjust_direction;
static volatile int32_t shadow_parameter = SHADOW_DEFAULT;
static volatile int32_t initial_adjust_pending;
static volatile uint32_t next_adjust_ms;
static volatile int32_t second_adjust_direction;
static volatile int32_t second_scanline_parameter = SCANLINE_SECOND_DEFAULT;
static volatile int32_t second_initial_adjust_pending;
static volatile uint32_t second_next_adjust_ms;
static volatile uint32_t settings_generation;
static volatile uint32_t settings_saved_generation;
static volatile uint32_t settings_last_change_ms;
static volatile uint32_t settings_retry_after_ms;
static volatile uint8_t settings_write_lock;

struct reset_chord_state {
    int32_t instance_id;
    uint32_t started_ms;
    uint8_t zl_down;
    uint8_t zr_down;
    uint8_t armed;
    uint8_t fired;
};
static struct reset_chord_state reset_chord_states[4];
static int reset_chord_states_initialized;
#endif

#ifdef CANOE_FILTER_DYNAMIC_CONTROL
static uint32_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint32_t)(now.tv_sec * 1000u + now.tv_nsec / 1000000u);
}

static int write_all(int fd, const char *data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(fd, data + offset, length - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return -1;
        offset += (size_t)written;
    }
    return 0;
}

static int save_settings_file(int32_t horizontal, int32_t vertical) {
    char temporary[160];
    char contents[64];
    int n = snprintf(temporary, sizeof(temporary), SETTINGS_DIR "/.settings.%ld.tmp",
                     (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(temporary)) return -1;
    n = snprintf(contents, sizeof(contents), "v1 %d %d\n", horizontal, vertical);
    if (n < 0 || (size_t)n >= sizeof(contents)) return -1;

    int fd = open(temporary, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
    if (fd < 0) return -1;
    int result = write_all(fd, contents, (size_t)n);
    if (!result && fsync(fd) != 0) result = -1;
    if (close(fd) != 0) result = -1;
    if (!result && rename(temporary, SETTINGS_PATH) != 0) result = -1;
    if (result) {
        (void)unlink(temporary);
        return -1;
    }

    int dirfd = open(SETTINGS_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd >= 0) {
        (void)fsync(dirfd);
        close(dirfd);
    }
    return 0;
}

static void persist_settings(int force) {
    uint32_t generation = __atomic_load_n(&settings_generation, __ATOMIC_ACQUIRE);
    uint32_t saved = __atomic_load_n(&settings_saved_generation, __ATOMIC_RELAXED);
    if (generation == saved) return;
    uint32_t now_ms = monotonic_ms();
    uint32_t changed_ms = __atomic_load_n(&settings_last_change_ms, __ATOMIC_RELAXED);
    if (!force && (int32_t)(now_ms - changed_ms) < (int32_t)SETTINGS_SAVE_DELAY_MS) return;
    uint32_t retry_after = __atomic_load_n(&settings_retry_after_ms, __ATOMIC_RELAXED);
    if (!force && retry_after && (int32_t)(now_ms - retry_after) < 0) return;
    if (__atomic_test_and_set(&settings_write_lock, __ATOMIC_ACQUIRE)) return;

    generation = __atomic_load_n(&settings_generation, __ATOMIC_ACQUIRE);
    int32_t horizontal = __atomic_load_n(&shadow_parameter, __ATOMIC_RELAXED);
    int32_t vertical = __atomic_load_n(&second_scanline_parameter, __ATOMIC_RELAXED);
    if (save_settings_file(horizontal, vertical) == 0) {
        __atomic_store_n(&settings_saved_generation, generation, __ATOMIC_RELEASE);
        __atomic_store_n(&settings_retry_after_ms, 0, __ATOMIC_RELAXED);
        if (log_fd >= 0) {
            char line[128];
            int len = snprintf(line, sizeof(line),
                               "event=settings_saved horizontal=%d vertical=%d\n",
                               horizontal, vertical);
            if (len > 0) (void)write(log_fd, line, (size_t)len);
        }
    } else if (log_fd >= 0) {
        const char line[] = "event=settings_save_failed\n";
        (void)write(log_fd, line, sizeof(line) - 1);
    }
    if (__atomic_load_n(&settings_saved_generation, __ATOMIC_RELAXED) != generation)
        __atomic_store_n(&settings_retry_after_ms, now_ms + SETTINGS_SAVE_DELAY_MS,
                         __ATOMIC_RELAXED);
    __atomic_clear(&settings_write_lock, __ATOMIC_RELEASE);
}

static void load_settings(void) {
    (void)mkdir(SETTINGS_DIR, 0755);
    int fd = open(SETTINGS_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;
    char contents[96];
    ssize_t length = read(fd, contents, sizeof(contents) - 1);
    close(fd);
    if (length <= 0 || length >= (ssize_t)sizeof(contents) - 1) return;
    contents[length] = 0;

    int horizontal = 0, vertical = 0, consumed = -1;
    if (sscanf(contents, "v1 %d %d %n", &horizontal, &vertical, &consumed) != 2 ||
        consumed < 0 || horizontal < SHADOW_MIN || horizontal > SHADOW_MAX ||
        vertical < SHADOW_MIN || vertical > SHADOW_MAX)
        return;
    for (int i = consumed; contents[i]; ++i)
        if (contents[i] != '\n' && contents[i] != '\r' && contents[i] != ' ' &&
            contents[i] != '\t')
            return;

    __atomic_store_n(&shadow_parameter, horizontal, __ATOMIC_RELAXED);
    __atomic_store_n(&second_scanline_parameter, vertical, __ATOMIC_RELAXED);
    if (log_fd >= 0) {
        char line[128];
        int len = snprintf(line, sizeof(line),
                           "event=settings_loaded horizontal=%d vertical=%d\n",
                           horizontal, vertical);
        if (len > 0) (void)write(log_fd, line, (size_t)len);
    }
}

static void mark_settings_changed(uint32_t now_ms) {
    __atomic_store_n(&settings_last_change_ms, now_ms, __ATOMIC_RELAXED);
    __atomic_add_fetch(&settings_generation, 1, __ATOMIC_RELEASE);
}

static void reset_to_defaults(void) {
    uint32_t now_ms = monotonic_ms();
    __atomic_store_n(&shadow_parameter, SHADOW_DEFAULT, __ATOMIC_RELAXED);
    __atomic_store_n(&second_scanline_parameter, SCANLINE_SECOND_DEFAULT, __ATOMIC_RELAXED);
    __atomic_store_n(&adjust_direction, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&initial_adjust_pending, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&next_adjust_ms, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&second_adjust_direction, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&second_initial_adjust_pending, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&second_next_adjust_ms, 0, __ATOMIC_RELAXED);
    mark_settings_changed(now_ms);
    persist_settings(1);
    if (log_fd >= 0) {
        char line[128];
        int len = snprintf(line, sizeof(line),
                           "event=settings_reset horizontal=%d vertical=%d\n",
                           SHADOW_DEFAULT, SCANLINE_SECOND_DEFAULT);
        if (len > 0) (void)write(log_fd, line, (size_t)len);
    }
}

static struct reset_chord_state *reset_state_for(int32_t instance_id, int create) {
    if (!reset_chord_states_initialized) {
        for (int i = 0; i < 4; ++i) reset_chord_states[i].instance_id = -1;
        reset_chord_states_initialized = 1;
    }
    for (int i = 0; i < 4; ++i)
        if (reset_chord_states[i].instance_id == instance_id)
            return &reset_chord_states[i];
    if (!create) return NULL;
    for (int i = 0; i < 4; ++i) {
        if (reset_chord_states[i].instance_id == -1) {
            reset_chord_states[i] = (struct reset_chord_state){ .instance_id = instance_id };
            return &reset_chord_states[i];
        }
    }
    return NULL;
}

static void set_reset_trigger(struct reset_chord_state *state, int axis, int16_t value,
                              uint32_t now_ms) {
    uint8_t *down = axis == ZL_AXIS ? &state->zl_down : &state->zr_down;
    if (*down ? value <= AXIS_EXIT_THRESHOLD : value >= AXIS_ENTER_THRESHOLD)
        *down = (uint8_t)(value > AXIS_EXIT_THRESHOLD);

    if (state->zl_down && state->zr_down) {
        if (!state->armed) {
            state->started_ms = now_ms;
            state->armed = 1;
            state->fired = 0;
        }
    } else {
        state->armed = 0;
        state->fired = 0;
    }
}

static void check_reset_chords(uint32_t now_ms) {
    for (int i = 0; i < 4; ++i) {
        struct reset_chord_state *state = &reset_chord_states[i];
        if (state->instance_id >= 0 && state->armed && !state->fired &&
            (uint32_t)(now_ms - state->started_ms) >= RESET_CHORD_HOLD_MS) {
            state->fired = 1;
            reset_to_defaults();
        }
    }
}

static void set_axis_direction(volatile int32_t *direction_state,
                               volatile int32_t *initial_pending,
                               volatile uint32_t *next_adjust,
                               int32_t direction) {
    int32_t was = __atomic_load_n(direction_state, __ATOMIC_RELAXED);
    if (direction == was) return;
    if (direction) {
        __atomic_store_n(next_adjust, 0, __ATOMIC_RELAXED);
        __atomic_store_n(initial_pending, 1, __ATOMIC_RELAXED);
        __atomic_store_n(direction_state, direction, __ATOMIC_RELAXED);
    } else {
        __atomic_store_n(direction_state, 0, __ATOMIC_RELAXED);
        __atomic_store_n(initial_pending, 0, __ATOMIC_RELAXED);
        __atomic_store_n(next_adjust, 0, __ATOMIC_RELAXED);
    }
}

static void adjust_held_parameter(volatile int32_t *direction_state,
                                  volatile int32_t *parameter,
                                  volatile int32_t *initial_pending,
                                  volatile uint32_t *next_adjust,
                                  int32_t step, const char *name) {
    int32_t direction = __atomic_load_n(direction_state, __ATOMIC_RELAXED);
    if (!direction) return;
    uint32_t now_ms = monotonic_ms();
    int first = __atomic_exchange_n(initial_pending, 0, __ATOMIC_RELAXED);
    uint32_t deadline = __atomic_load_n(next_adjust, __ATOMIC_RELAXED);
    int due = deadline && (int32_t)(now_ms - deadline) >= 0;
    if (!first && !due) return;
    uint32_t next = now_ms + (first ? KEY_DELAY_MS : REPEAT_INTERVAL_MS);
    if (first) {
        __atomic_store_n(next_adjust, next, __ATOMIC_RELAXED);
    } else if (!__atomic_compare_exchange_n(next_adjust, &deadline, next, 0,
                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        return;
    }
    direction = __atomic_load_n(direction_state, __ATOMIC_RELAXED);
    if (!direction) return;
    int32_t value = __atomic_load_n(parameter, __ATOMIC_RELAXED);
    int32_t updated = value + direction * step;
    if (updated < SHADOW_MIN) updated = SHADOW_MIN;
    if (updated > SHADOW_MAX) updated = SHADOW_MAX;
    __atomic_store_n(parameter, updated, __ATOMIC_RELAXED);
    if (updated != value && log_fd >= 0) {
        char line[112];
        int len = snprintf(line, sizeof(line), "event=adjust parameter=%s value=%d direction=%d\n",
                           name, updated, direction);
        if (len > 0) (void)write(log_fd, line, (size_t)len);
    }
    if (updated != value) mark_settings_changed(now_ms);
}
#endif

/* Observe the per-vertex vec2 data supplied as aScanline by Canoe's GL pass. */
void glVertexAttribPointer(unsigned int index, int size, unsigned int type,
                           unsigned char normalized, int stride, const void *pointer) {
    if (!real_gl_vertex_attrib_pointer)
        real_gl_vertex_attrib_pointer =
            (gl_vertex_attrib_pointer_fn)dlsym(RTLD_NEXT, "glVertexAttribPointer");
    if (log_fd >= 0) {
        uint32_t n = __atomic_fetch_add(&logged_shader_attributes, 1, __ATOMIC_RELAXED);
        if (n < 16) {
            char line[256];
            int len;
            if ((uintptr_t)pointer == 0x004da800u && size == 2 && type == 0x1406u) {
                const float *v = (const float *)pointer;
                len = snprintf(line, sizeof(line),
                    "event=shader_scanline_attribute n=%u index=%u pointer=%p size=%d type=%x stride=%d sample=%g,%g,%g,%g,%g,%g,%g,%g\n",
                    n, index, pointer, size, type, stride,
                    v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
            } else {
                len = snprintf(line, sizeof(line),
                    "event=gl_vertex_attribute n=%u index=%u pointer=%p size=%d type=%x stride=%d\n",
                    n, index, pointer, size, type, stride);
            }
            if (len > 0) (void)write(log_fd, line, (size_t)len < sizeof(line) ? (size_t)len : sizeof(line));
        }
    }
    if (real_gl_vertex_attrib_pointer)
        real_gl_vertex_attrib_pointer(index, size, type, normalized, stride, pointer);
}

static int supported_canoe(void) {
    char path[512];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0) return 0;
    path[n] = 0;
    char *base = strrchr(path, '/');
    if (!base || strcmp(base + 1, "canoe-shvc")) return 0;
    struct stat st;
    return !stat("/proc/self/exe", &st) && st.st_size == 1836012;
}

static void record_worker(const volatile uint8_t *p) {
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
    adjust_held_parameter(&adjust_direction, &shadow_parameter, &initial_adjust_pending,
                          &next_adjust_ms, SHADOW_STEP, "horizontal_120");
    adjust_held_parameter(&second_adjust_direction, &second_scanline_parameter,
                          &second_initial_adjust_pending, &second_next_adjust_ms,
                          SHADOW_STEP, "vertical_85");
    persist_settings(0);
    __atomic_store_n((volatile uint32_t *)(p + 0x1c),
                     (uint32_t)__atomic_load_n(&shadow_parameter, __ATOMIC_RELAXED),
                     __ATOMIC_RELAXED);
    __atomic_store_n((volatile uint32_t *)(p + 0x24),
                     (uint32_t)__atomic_load_n(&second_scanline_parameter, __ATOMIC_RELAXED),
                     __ATOMIC_RELAXED);
#endif
    uint32_t n = __atomic_fetch_add(&logged, 1, __ATOMIC_RELAXED);
    if (n >= 12 || log_fd < 0) return;
    const volatile uint32_t *coeff = (const volatile uint32_t *)(p + 0x1c);
    char line[512];
    int len = snprintf(line, sizeof(line),
        "event=worker_call n=%u tid=%ld desc=%p dst=%p src=%p width=%u rows=%u flags=%02x,%02x,%02x,%02x mode=%02x coeff_raw=%08x,%08x,%08x,%08x\n",
        n, (long)syscall(SYS_gettid), (const void *)p,
        (void *)(uintptr_t)*(const volatile uint32_t *)(p + 4),
        (void *)(uintptr_t)*(const volatile uint32_t *)(p + 8),
        *(const volatile uint32_t *)(p + 0x0c),
        *(const volatile uint32_t *)(p + 0x10),
        p[0x14], p[0x15], p[0x16], p[0x17],
        p[0x18], coeff[0], coeff[1], coeff[2], coeff[3]);
    if (len > 0) (void)write(log_fd, line, (size_t)len < sizeof(line) ? (size_t)len : sizeof(line));
}

/* The second stick's horizontal axis adjusts the first signed worker input.
 * Canoe's other SDL events pass through unchanged. */
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
int SDL_PollEvent(void *opaque_event) {
    SDL_Event_Compat *event = opaque_event;
    if (!real_poll)
        real_poll = (poll_event_fn)dlsym(RTLD_NEXT, "SDL_PollEvent");
    if (!real_poll) return 0;
    int result = real_poll(opaque_event);
    uint32_t now_ms = monotonic_ms();
    if (result && event) {
        if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
            set_axis_direction(&adjust_direction, &initial_adjust_pending, &next_adjust_ms, 0);
            set_axis_direction(&second_adjust_direction, &second_initial_adjust_pending,
                               &second_next_adjust_ms, 0);
            struct reset_chord_state *state = reset_state_for(event->cdevice.which, 0);
            if (state) *state = (struct reset_chord_state){ .instance_id = -1 };
        } else if (event->type == SDL_CONTROLLERAXISMOTION) {
            if (event->caxis.axis == SDL_RIGHTX) {
                int32_t was = __atomic_load_n(&adjust_direction, __ATOMIC_RELAXED);
                int32_t direction = was;
                if (event->caxis.value > AXIS_ENTER_THRESHOLD)
                    direction = 1;
                else if (event->caxis.value < -AXIS_ENTER_THRESHOLD)
                    direction = -1;
                else if (event->caxis.value < AXIS_EXIT_THRESHOLD &&
                         event->caxis.value > -AXIS_EXIT_THRESHOLD)
                    direction = 0;
                set_axis_direction(&adjust_direction, &initial_adjust_pending,
                                   &next_adjust_ms, direction);
            } else if (event->caxis.axis == SDL_RIGHTY) {
                int32_t was = __atomic_load_n(&second_adjust_direction, __ATOMIC_RELAXED);
                int32_t direction = was;
                /* SDL reports up as negative Y; map up to increasing the coefficient. */
                if (event->caxis.value < -AXIS_ENTER_THRESHOLD)
                    direction = 1;
                else if (event->caxis.value > AXIS_ENTER_THRESHOLD)
                    direction = -1;
                else if (event->caxis.value < AXIS_EXIT_THRESHOLD &&
                         event->caxis.value > -AXIS_EXIT_THRESHOLD)
                    direction = 0;
                set_axis_direction(&second_adjust_direction, &second_initial_adjust_pending,
                                   &second_next_adjust_ms, direction);
            } else if (event->caxis.axis == ZL_AXIS || event->caxis.axis == ZR_AXIS) {
                struct reset_chord_state *state =
                    reset_state_for((int32_t)event->caxis.which, 1);
                if (state) set_reset_trigger(state, event->caxis.axis, event->caxis.value, now_ms);
            }
        }
    }
    check_reset_chords(now_ms);
    persist_settings(0);
    return result;
}
#endif

static void observed_worker(void *arg) {
    record_worker((const volatile uint8_t *)arg);
    ((void (*)(void *))worker_hook.trampoline)(arg);
}

__attribute__((constructor)) static void probe_start(void) {
    const char *enabled = getenv("CANOE_OBSERVER_ENABLE");
    if (!enabled || strcmp(enabled, "1") || !supported_canoe()) return;
    const char *log_enabled = getenv("CANOE_FILTER_WORKER_LOG");
    if (log_enabled && !strcmp(log_enabled, "1")) {
        (void)mkdir("/tmp/canoe-filter-worker-probe", 0755);
        log_fd = open("/tmp/canoe-filter-worker-probe/events.log",
                      O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, 0644);
    }
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
    load_settings();
#endif
    const uint32_t expected[] = {0xe92d4ff0u, 0xe5d0b018u};
    if (arm_hook_install(&worker_hook, WORKER_ENTRY, (void *)observed_worker,
                         expected, sizeof(expected) / sizeof(expected[0]))) {
        if (log_fd >= 0) {
            char line[128];
            int n = snprintf(line, sizeof(line), "event=hook_refused errno=%d\n", errno);
            if (n > 0) (void)write(log_fd, line, (size_t)n);
            close(log_fd);
            log_fd = -1;
        }
        return;
    }
    const char ready[] = "event=probe_loaded hook=0x00163084\n";
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
    if (log_fd >= 0) {
        char test_note[384];
        int note_len = snprintf(test_note, sizeof(test_note),
            "event=adjustment control=right-stick-horizontal input_offset=0x1c signed_i32 initial=%d range=0..199 step=10 key_delay_ms=%u repeat_ms=%u\n"
            "event=adjustment control=right-stick-vertical-up input_offset=0x24 signed_i32 initial=%d range=0..199 step=10 key_delay_ms=%u repeat_ms=%u\n",
            SHADOW_DEFAULT, KEY_DELAY_MS, REPEAT_INTERVAL_MS,
            SCANLINE_SECOND_DEFAULT, KEY_DELAY_MS, REPEAT_INTERVAL_MS);
        if (note_len > 0)
            (void)write(log_fd, test_note,
                        (size_t)note_len < sizeof(test_note) ? (size_t)note_len : sizeof(test_note));
    }
#endif
    if (log_fd >= 0) (void)write(log_fd, ready, sizeof(ready) - 1);
}

__attribute__((destructor)) static void probe_stop(void) {
#ifdef CANOE_FILTER_DYNAMIC_CONTROL
    persist_settings(1);
#endif
    if (log_fd >= 0) close(log_fd);
}
