#include "canoe_scanline.h"

void canoe_scanline_transform(float *destination, const float *source,
                              size_t pair_count, float strength) {
    const float mean_x = (1.375f + 0.675f + 0.85f) / 3.0f;
    const float mean_y = (-0.35f + 0.25f + 0.1f) / 3.0f;
    if (!destination || !source)
        return;
    for (size_t i = 0; i < pair_count; ++i) {
        destination[i * 2] = mean_x + (source[i * 2] - mean_x) * strength;
        destination[i * 2 + 1] = mean_y + (source[i * 2 + 1] - mean_y) * strength;
    }
}
