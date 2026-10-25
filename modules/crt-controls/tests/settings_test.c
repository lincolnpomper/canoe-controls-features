#define _GNU_SOURCE
#define CANOE_FILTER_DYNAMIC_CONTROL 1
#define SETTINGS_DIR "."
#define SETTINGS_PATH "./settings.conf"
#include "../src/canoe_crt_control.c"

#include <assert.h>

int arm_hook_install(struct arm_hook *hook, void *entry, void *replacement,
                     const uint32_t *expected, size_t expected_words) {
    (void)hook; (void)entry; (void)replacement; (void)expected; (void)expected_words;
    return -1;
}

int arm_hook_remove(struct arm_hook *hook) {
    (void)hook;
    return -1;
}

static void write_settings(const char *text) {
    FILE *file = fopen(SETTINGS_PATH, "wb");
    assert(file);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(fclose(file) == 0);
}

static void assert_settings(const char *expected) {
    char actual[64] = {0};
    FILE *file = fopen(SETTINGS_PATH, "rb");
    assert(file);
    assert(fread(actual, 1, sizeof(actual) - 1, file) > 0);
    assert(fclose(file) == 0);
    assert(strcmp(actual, expected) == 0);
}

int main(void) {
    load_settings();
    assert(shadow_parameter == 114 && second_scanline_parameter == 85);

    write_settings("v1 74 115\n");
    load_settings();
    assert(shadow_parameter == 74 && second_scanline_parameter == 115);

    write_settings("v1 0 199\n");
    load_settings();
    assert(shadow_parameter == 0 && second_scanline_parameter == 199);

    shadow_parameter = 20;
    second_scanline_parameter = 30;
    write_settings("v1 200 20\n");
    load_settings();
    assert(shadow_parameter == 20 && second_scanline_parameter == 30);
    write_settings("v1 20 30 trailing\n");
    load_settings();
    assert(shadow_parameter == 20 && second_scanline_parameter == 30);

    shadow_parameter = 47;
    second_scanline_parameter = 168;
    mark_settings_changed(monotonic_ms() - SETTINGS_SAVE_DELAY_MS - 1u);
    persist_settings(0);
    assert_settings("v1 47 168\n");

    reset_to_defaults();
    assert(shadow_parameter == 114 && second_scanline_parameter == 85);
    assert_settings("v1 114 85\n");
    puts("PASS: CRT settings validation, load, delayed save and reset persistence");
    return 0;
}
