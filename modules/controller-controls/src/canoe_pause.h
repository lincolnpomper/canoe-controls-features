#ifndef CANOE_PAUSE_H
#define CANOE_PAUSE_H

#include <stdint.h>

/* Invoke only on the thread polling Canoe's SDL input. */
void canoe_pause_toggle(int32_t instance_id);

/* Guard all native state reads/calls behind the verified executable profile. */
int canoe_native_supported(void);
int canoe_game_ready(void);
void canoe_controls_record(const char *event, int32_t instance_id);

#endif
