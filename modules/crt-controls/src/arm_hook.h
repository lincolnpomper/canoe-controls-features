#ifndef CANOE_ARM_HOOK_H
#define CANOE_ARM_HOOK_H
#include <stdint.h>
#include <stddef.h>
struct arm_hook {
    uint32_t *entry;
    uint32_t original[2];
    void *trampoline;
    size_t allocation;
    int installed;
};
/* ARM state only. The first two instructions must be audited as relocatable
 * without PC-relative operands. Install/remove only while no thread executes
 * the target (observer installs before main; self-test is single-threaded). */
int arm_hook_install(struct arm_hook *h, void *entry, void *replacement,
                     const uint32_t *expected, size_t expected_words);
int arm_hook_remove(struct arm_hook *h);
#endif
