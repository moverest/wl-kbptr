// SPDX-License-Identifier: GPL-3.0-only

#include "config.h"
#include "settings_overlay.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 64

static struct config_field_ref *find_field(
    struct config_field_ref *fields, int num, const char *s, const char *n
) {
    for (int i = 0; i < num; i++) {
        if (strcmp(fields[i].section, s) == 0 &&
            strcmp(fields[i].name, n) == 0) {
            return &fields[i];
        }
    }

    return NULL;
}

static void set_field(
    struct config *config, const char *section, const char *name,
    const char *value
) {
    struct config_loader loader;
    config_loader_init(&loader, config);
    assert(config_loader_enter_section(&loader, (char *)section) == 0);
    assert(config_loader_load_field(&loader, (char *)name, (char *)value) == 0);
}

// A value that has been formatted must parse back to the exact same bytes.
static void test_round_trip() {
    struct config config;
    config_set_default(&config);

    struct config_field_ref fields[MAX_FIELDS];
    const int num = config_editable_fields(&config, fields, MAX_FIELDS);
    assert(num > 0);

    struct {
        const char *section;
        const char *name;
        const char *input;
        const char *expected;
        size_t      size;
    } cases[] = {
        {"mode_tile", "selectable_bg_color", "#0304", "#00330044",
         sizeof(uint32_t)},
        {"mode_tile", "label_color", "#fffd", "#ffffffdd", sizeof(uint32_t)},
        {"mode_bisect", "unselectable_bg_color", "#11223344", "#11223344",
         sizeof(uint32_t)},
        {"mode_bisect", "label_padding", "12", "12", sizeof(double)},
        {"mode_split", "pointer_size", "20.5", "20.5", sizeof(double)},
        {"mode_tile", "label_font_size", "8 50% 100", "8 50% 100",
         sizeof(struct relative_font_size)},
        {"mode_floating", "label_font_size", "12 50% 100", "12 50% 100",
         sizeof(struct relative_font_size)},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        set_field(&config, cases[i].section, cases[i].name, cases[i].input);

        struct config_field_ref *field =
            find_field(fields, num, cases[i].section, cases[i].name);
        assert(field != NULL);

        char formatted[64];
        config_format_field(field, formatted, sizeof(formatted));

        if (strcmp(formatted, cases[i].expected) != 0) {
            fprintf(
                stderr, "%s.%s: formatted '%s', expected '%s'\n",
                cases[i].section, cases[i].name, formatted, cases[i].expected
            );
            exit(1);
        }

        // Reparsing what we wrote must land on the identical value.
        char before[sizeof(struct relative_font_size)];
        memcpy(before, field->value, cases[i].size);

        set_field(&config, cases[i].section, cases[i].name, formatted);

        if (memcmp(before, field->value, cases[i].size) != 0) {
            fprintf(
                stderr, "%s.%s: '%s' did not round trip\n", cases[i].section,
                cases[i].name, formatted
            );
            exit(1);
        }
    }

    config_free_values(&config);
}

// Colors and sizes are picked up automatically; strings and enums are not.
static void test_editable_selection() {
    struct config config;
    config_set_default(&config);

    struct config_field_ref fields[MAX_FIELDS];
    const int num = config_editable_fields(&config, fields, MAX_FIELDS);

    assert(find_field(fields, num, "mode_tile", "label_color") != NULL);
    assert(find_field(fields, num, "mode_tile", "label_font_size") != NULL);
    assert(find_field(fields, num, "mode_split", "pointer_size") != NULL);

    assert(find_field(fields, num, "mode_tile", "label_symbols") == NULL);
    assert(find_field(fields, num, "mode_tile", "label_font_family") == NULL);
    assert(find_field(fields, num, "general", "modes") == NULL);
    assert(find_field(fields, num, "mode_click", "button") == NULL);

    int num_colors = 0;
    for (int i = 0; i < num; i++) {
        if (fields[i].kind == CONFIG_FIELD_COLOR) {
            num_colors++;
        }
    }
    assert(num_colors == 24);

    config_free_values(&config);
}

static char *patch(
    const char *input, const struct config_field_ref *changes,
    const char **values, int num_changes
) {
    FILE *in =
        input == NULL ? NULL : fmemopen((void *)input, strlen(input), "r");

    char  *buf  = NULL;
    size_t size = 0;
    FILE  *out  = open_memstream(&buf, &size);
    assert(out != NULL);

    settings_overlay_patch_config(in, out, changes, values, num_changes);

    fclose(out);
    if (in != NULL) {
        fclose(in);
    }

    return buf;
}

static void expect_eq(const char *got, const char *want, const char *what) {
    if (strcmp(got, want) != 0) {
        fprintf(
            stderr, "%s:\n--- got ---\n%s\n--- want ---\n%s\n", what, got, want
        );
        exit(1);
    }
}

// Existing values are replaced in place; comments, blank lines, ordering and
// unrelated keys survive untouched.
static void test_patch_preserves_file() {
    const char *input = "# my config\n"
                        "\n"
                        "[general]\n"
                        "modes=tile,bisect\n"
                        "\n"
                        "[mode_tile]\n"
                        "# the label color\n"
                        "label_color = #fffd\n"
                        "selectable_bg_color=#0304\n"
                        "label_symbols=asdfghjkl\n";

    struct config_field_ref changes[] = {
        {.section = "mode_tile", .name = "label_color"},
        {.section = "mode_tile", .name = "selectable_bg_color"},
    };
    const char *values[] = {"#ff0000ff", "#00ff0080"};

    char *got = patch(input, changes, values, 2);

    expect_eq(
        got,
        "# my config\n"
        "\n"
        "[general]\n"
        "modes=tile,bisect\n"
        "\n"
        "[mode_tile]\n"
        "# the label color\n"
        "label_color=#ff0000ff\n"
        "selectable_bg_color=#00ff0080\n"
        "label_symbols=asdfghjkl\n",
        "patch in place"
    );

    free(got);
}

// A field that is not already in the file is appended as its own section.
static void test_patch_appends_missing() {
    const char *input = "[mode_tile]\n"
                        "label_color=#fffd\n";

    struct config_field_ref changes[] = {
        {.section = "mode_tile", .name = "label_color"},
        {.section = "mode_bisect", .name = "pointer_size"},
        {.section = "mode_bisect", .name = "label_padding"},
    };
    const char *values[] = {"#ff0000ff", "25", "14"};

    char *got = patch(input, changes, values, 3);

    expect_eq(
        got,
        "[mode_tile]\n"
        "label_color=#ff0000ff\n"
        "\n"
        "[mode_bisect]\n"
        "pointer_size=25\n"
        "label_padding=14\n",
        "append missing"
    );

    free(got);
}

// With no config file at all, the patcher writes a whole one.
static void test_patch_no_existing_file() {
    struct config_field_ref changes[] = {
        {.section = "mode_tile", .name = "label_color"},
    };
    const char *values[] = {"#abcdef01"};

    char *got = patch(NULL, changes, values, 1);

    expect_eq(
        got, "\n[mode_tile]\nlabel_color=#abcdef01\n", "no existing file"
    );

    free(got);
}

// A key with the same name in another section must not be touched.
static void test_patch_respects_sections() {
    const char *input = "[mode_tile]\n"
                        "label_color=#111111ff\n"
                        "[mode_floating]\n"
                        "label_color=#222222ff\n";

    struct config_field_ref changes[] = {
        {.section = "mode_floating", .name = "label_color"},
    };
    const char *values[] = {"#333333ff"};

    char *got = patch(input, changes, values, 1);

    expect_eq(
        got,
        "[mode_tile]\n"
        "label_color=#111111ff\n"
        "[mode_floating]\n"
        "label_color=#333333ff\n",
        "respect sections"
    );

    free(got);
}

int main() {
    test_round_trip();
    test_editable_selection();
    test_patch_preserves_file();
    test_patch_appends_missing();
    test_patch_no_existing_file();
    test_patch_respects_sections();

    puts("ok");
    return 0;
}
