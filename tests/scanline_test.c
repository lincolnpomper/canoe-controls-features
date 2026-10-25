#include "canoe_scanline.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int close_to(float a, float b) { return fabsf(a - b) < 0.00001f; }

int main(void) {
    const float source[] = {1.375f, -0.35f, 0.675f, 0.25f, 0.85f, 0.1f};
    float output[6] = {0};
    canoe_scanline_transform(output, source, 3, 1.0f);
    for (int i = 0; i < 6; ++i)
        assert(close_to(output[i], source[i]));
    canoe_scanline_transform(output, source, 3, 0.0f);
    for (int i = 0; i < 3; ++i) {
        assert(close_to(output[i * 2], 2.9f / 3.0f));
        assert(close_to(output[i * 2 + 1], 0.0f));
    }
    canoe_scanline_transform(output, source, 3, 1.25f);
    for (int component = 0; component < 2; ++component) {
        float original_mean = 0.0f, adjusted_mean = 0.0f;
        for (int i = 0; i < 3; ++i) {
            original_mean += source[i * 2 + component] / 3.0f;
            adjusted_mean += output[i * 2 + component] / 3.0f;
        }
        assert(close_to(original_mean, adjusted_mean));
    }
    assert(output[0] > source[0]);
    assert(output[2] < source[2]);
    assert(output[4] < source[4]);
    assert(source[0] == 1.375f);
    puts("PASS: baseline, zero strength, mean preservation, contrast expansion, source immutability");
    return 0;
}
