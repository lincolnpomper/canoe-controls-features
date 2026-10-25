#ifndef CANOE_SCANLINE_H
#define CANOE_SCANLINE_H

#include <stddef.h>

#define CANOE_SCANLINE_PAIR_COUNT 1344

void canoe_scanline_transform(float *destination, const float *source,
                              size_t pair_count, float strength);

#endif
