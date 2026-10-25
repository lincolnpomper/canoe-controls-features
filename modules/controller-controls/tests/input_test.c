#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef union {
    uint32_t type;
    struct {
        uint32_t type, timestamp;
        int32_t which;
        uint8_t axis, padding[3];
        int16_t value;
        uint16_t padding4;
    } axis;
    struct { uint32_t type, timestamp; int32_t which; } device;
    uint8_t pad[56];
} Event;

int SDL_PollEvent(void *);
uint8_t SDL_GameControllerGetButton(void *, int);

static Event queued;
static int queued_result;
static int toggles;
static int32_t last_toggle_id;
static uint8_t physical_button;
static int fast_requested;

static int fake_poll(void *out) {
    if (queued_result && out)
        memcpy(out, &queued, sizeof(queued));
    return queued_result;
}
static uint8_t fake_button(void *controller, int button) {
    (void)controller; (void)button;
    return physical_button;
}
static void *fake_joystick(void *controller) { return controller; }
static int32_t fake_instance(void *joystick) { return *(int32_t *)joystick; }

void *mock_dlsym(void *handle, const char *name) {
    (void)handle;
    if (!strcmp(name, "SDL_PollEvent")) return (void *)fake_poll;
    if (!strcmp(name, "SDL_GameControllerGetButton")) return (void *)fake_button;
    if (!strcmp(name, "SDL_GameControllerGetJoystick")) return (void *)fake_joystick;
    if (!strcmp(name, "SDL_JoystickInstanceID")) return (void *)fake_instance;
    return NULL;
}
void canoe_pause_toggle(int32_t instance) {
    ++toggles;
    last_toggle_id = instance;
}
void canoe_fast_forward_set(int held, int32_t instance) {
    (void)instance;
    fast_requested = held;
}
static void axis(int32_t id, uint8_t which, int16_t value) {
    queued = (Event){0};
    queued.axis.type = 0x650;
    queued.axis.which = id;
    queued.axis.axis = which;
    queued.axis.value = value;
    queued_result = 1;
    Event out;
    assert(SDL_PollEvent(&out) == 1);
    assert(!memcmp(&out, &queued, sizeof(out))); /* Preserve original event. */
}
static void remove_controller(int32_t id) {
    queued = (Event){0};
    queued.device.type = 0x654;
    queued.device.which = id;
    queued_result = 1;
    Event out;
    assert(SDL_PollEvent(&out) == 1);
}
int main(void) {
    int32_t p1 = 42, p2 = 99;
    axis(p1, 0, 20000);
    assert(SDL_GameControllerGetButton(&p1, 14));
    assert(!SDL_GameControllerGetButton(&p2, 14));
    axis(p1, 0, 15000);
    assert(SDL_GameControllerGetButton(&p1, 14));
    axis(p1, 0, 11000);
    assert(!SDL_GameControllerGetButton(&p1, 14));
    axis(p1, 0, -22000);
    axis(p1, 1, -22000);
    assert(SDL_GameControllerGetButton(&p1, 13));
    assert(SDL_GameControllerGetButton(&p1, 11));
    axis(p1, 1, 22000);
    assert(!SDL_GameControllerGetButton(&p1, 11));
    assert(SDL_GameControllerGetButton(&p1, 12));
    physical_button = 1;
    assert(SDL_GameControllerGetButton(&p2, 14));
    assert(SDL_GameControllerGetButton(&p1, 0));
    physical_button = 0;

    axis(p1, 4, 32767);
    assert(toggles == 1 && last_toggle_id == p1);
    for (int i = 0; i < 50; ++i) axis(p1, 4, 32767);
    axis(p1, 4, 15000);
    axis(p1, 4, 20000);
    assert(toggles == 1); /* Holding/jitter must not toggle repeatedly. */
    axis(p1, 5, 32767);
    assert(toggles == 1); /* ZR is not the pause shortcut. */
    assert(fast_requested);
    axis(p2, 4, 32767);
    assert(toggles == 2 && last_toggle_id == p2);
    axis(p1, 4, 0);
    axis(p1, 4, 32767);
    assert(toggles == 3 && last_toggle_id == p1);
    axis(p2, 4, 32767);
    assert(toggles == 3); /* Player 1 release cannot rearm player 2. */
    remove_controller(p1);
    assert(!fast_requested);
    assert(!SDL_GameControllerGetButton(&p1, 13));
    assert(!SDL_GameControllerGetButton(&p1, 12));
    axis(p1, 4, 32767);
    assert(toggles == 4); /* Reconnect starts with fresh state. */
    for (int32_t id = 100; id < 110; ++id) {
        axis(id, 4, 32767);
        assert(last_toggle_id == id);
        remove_controller(id); /* Reuse slots instead of exhausting them. */
    }
    queued_result = 0;
    Event unchanged = {.type = 123};
    assert(!SDL_PollEvent(&unchanged) && unchanged.type == 123);
    assert(toggles == 14);
    axis(p1, 5, 32767);
    axis(p2, 5, 32767);
    assert(fast_requested);
    axis(p1, 5, 0);
    assert(fast_requested); /* Other controller is still holding ZR. */
    axis(p2, 5, 15000);
    assert(fast_requested);
    axis(p2, 5, 11000);
    assert(!fast_requested);
    axis(p1, 5, 32767);
    assert(fast_requested);
    remove_controller(p1);
    assert(!fast_requested && toggles == 14);
    puts("PASS: ZL edges/hold/release, shared ZR held state/release/disconnect, independent controllers, reconnect, analog hysteresis/diagonals, physical buttons and event preservation");
    return 0;
}
