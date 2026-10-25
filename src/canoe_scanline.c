#define _GNU_SOURCE

#include "canoe_scanline.h"
#include "canoe_pause.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>

enum { GL_FLOAT_VALUE = 0x1406 };
#define CANOE_SCANLINE_ADDRESS ((const void *)(uintptr_t)0x004da800u)

typedef int (*get_attrib_location_fn)(unsigned int, const char *);
typedef void (*vertex_attrib_pointer_fn)(unsigned int, int, unsigned int,
                                         unsigned char, int, const void *);
static get_attrib_location_fn real_get_attrib_location;
static vertex_attrib_pointer_fn real_vertex_attrib_pointer;
static int scanline_attrib_location = -1;
static float adjusted_scanlines[CANOE_SCANLINE_PAIR_COUNT * 2];
static int strength_loaded;
static int adjustment_logged;
static float scanline_strength = 1.25f;

int glGetAttribLocation(unsigned int program, const char *name) {
    if (!real_get_attrib_location)
        real_get_attrib_location = (get_attrib_location_fn)
            dlsym(RTLD_NEXT, "glGetAttribLocation");
    if (!real_get_attrib_location)
        return -1;
    int location = real_get_attrib_location(program, name);
    if (name && __builtin_strcmp(name, "aScanline") == 0)
        scanline_attrib_location = location;
    return location;
}

static void load_strength(void) {
    if (strength_loaded)
        return;
    strength_loaded = 1;
    const char *setting = getenv("CANOE_SCANLINE_STRENGTH");
    if (!setting || !*setting)
        return;
    char *end = NULL;
    float parsed = strtof(setting, &end);
    if (end != setting && *end == '\0' && parsed >= 0.0f && parsed <= 2.0f)
        scanline_strength = parsed;
}

void glVertexAttribPointer(unsigned int index, int size, unsigned int type,
                           unsigned char normalized, int stride,
                           const void *pointer) {
    if (!real_vertex_attrib_pointer)
        real_vertex_attrib_pointer = (vertex_attrib_pointer_fn)
            dlsym(RTLD_NEXT, "glVertexAttribPointer");
    if (!real_vertex_attrib_pointer)
        return;

    if (pointer == CANOE_SCANLINE_ADDRESS && size == 2 &&
        type == GL_FLOAT_VALUE && !normalized && stride == 0 &&
        scanline_attrib_location >= 0 && index == (unsigned)scanline_attrib_location &&
        canoe_native_supported()) {
        load_strength();
        if (scanline_strength != 1.0f) {
            canoe_scanline_transform(adjusted_scanlines,
                (const float *)CANOE_SCANLINE_ADDRESS,
                CANOE_SCANLINE_PAIR_COUNT, scanline_strength);
            pointer = adjusted_scanlines;
            if (!adjustment_logged) {
                canoe_controls_record("scanline_strength_applied", -1);
                adjustment_logged = 1;
            }
        }
    }
    real_vertex_attrib_pointer(index, size, type, normalized, stride, pointer);
}
