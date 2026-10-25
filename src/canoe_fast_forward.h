#ifndef CANOE_FAST_FORWARD_H
#define CANOE_FAST_FORWARD_H
#include <stdint.h>

/* Aggregate held state of every connected controller; call on input thread. */
void canoe_fast_forward_set(int held, int32_t instance_id);
#endif
