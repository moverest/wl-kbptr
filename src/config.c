#include "config.h"

#include "log.h"
#include "state.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * `field_color_parse` parses color field values, e.g.:
 *  - 6-digit hex color code (RRGGBB): `#123456`, `123456`
 *  - 8-digit hex color code (RRGGBBAA): `#12345678`, `12345678`
 *  - 3-digit hex color code (RGB): `#123`, `123`
 *  - 4-digit hex color code (RGBA): `#1234`, `1234`
 */
static int parse_color(void *dest, char *value) {
    if (value[0] == '#') {
        value++;
    }

    char digits[8];
    int  len = 0;
    for (int i = 0; value[i] != '\0'; i++) {
        if (++len > 8) {
            return 1;
        }

        char c = value[i];
        if (c >= '0' && c <= '9') {
            digits[i] = c - '0';
            continue;
        }

        c |= 1 << 5; // Makes the character lower case.
        if (c >= 'a' && c <= 'f') {
            digits[i] = c - 'a' + 10;
            continue;
        }

        return 2;
    }

    switch (len) {
    case 8:;
        *((uint32_t *)dest) = digits[7] | digits[6] << 4 |
                              digits[5] << (4 * 2) | digits[4] << (4 * 3) |
                              digits[3] << (4 * 4) | digits[2] << (4 * 5) |
                              digits[1] << (4 * 6) | digits[0] << (4 * 7);
        break;

    case 6:
        *((uint32_t *)dest) = 0xff | digits[5] << (4 * 2) |
                              digits[4] << (4 * 3) | digits[3] << (4 * 4) |
                              digits[2] << (4 * 5) | digits[1] << (4 * 6) |
                              digits[0] << (4 * 7);
        break;

    case 4:
        *((uint32_t *)dest) = digits[3] | digits[3] << 4 |
                              digits[2] << (4 * 2) | digits[2] << (4 * 3) |
                              digits[1] << (4 * 4) | digits[1] << (4 * 5) |
                              digits[0] << (4 * 6) | digits[0] << (4 * 7);
        break;

    case 3:
        *((uint32_t *)dest) = 0xff | digits[2] << (4 * 2) |
                              digits[2] << (4 * 3) | digits[1] << (4 * 4) |
                              digits[1] << (4 * 5) | digits[0] << (4 * 6) |
                              digits[0] << (4 * 7);
        break;

    default:
        return 4;
    }

    return 0;
}

static int parse_double(void *dest, char *value) {
    *((double *)dest) = atof(value);
    // TODO: handle errors better.
    return 0;
}

static int parse_uint8(void *dest, char *value) {
    int decoded = atoi(value);
    if (decoded < 0 || decoded >= 256) {
        LOG_ERR("Value should be between 0 and 255 (included).");
        return 1;
    }

    *((uint8_t *)dest) = (uint8_t)decoded;
    return 0;
}

static int parse_relative_font_size(void *dest, char *value) {
    struct relative_font_size *rfs = dest;

    static const char delims[] = " \t";

    int  s_len = strlen(value);
    char buf[s_len + 1];
    strcpy(buf, value);
    buf[s_len] = '\0';

    char *strtok_p;
    char *token = strtok_r(buf, delims, &strtok_p);

    if (token == NULL) {
        LOG_ERR("No minimum size.");
        return 1;
    }

    errno    = 0;
    rfs->min = strtod(token, NULL);
    if (errno) {
        LOG_ERR("Invalid minimum size.");
        return 1;
    }

    token = strtok_r(NULL, delims, &strtok_p);
    if (token == NULL) {
        LOG_ERR("No relative size.");
        return 1;
    }

    errno           = 0;
    rfs->proportion = strtod(token, NULL);
    if (token[strlen(token) - 1] == '%') {
        rfs->proportion /= 100.;
    }
    if (errno) {
        LOG_ERR("Invalid relative size.");
        return 1;
    }

    token = strtok_r(NULL, delims, &strtok_p);
    if (token == NULL) {
        LOG_ERR("No maximum size.");
        return 1;
    }

    errno    = 0;
    rfs->max = strtod(token, NULL);
    if (errno) {
        LOG_ERR("Invalid maximum size.");
        return 1;
    }

    if (rfs->min < 0) {
        LOG_ERR("Minimum size can't be negative.");
        return 1;
    }

    if (rfs->max < 0) {
        LOG_ERR("Maximum size can't be negative.");
        return 1;
    }

    if (rfs->proportion < 0) {
        LOG_ERR("Proportion can't be negative.");
        return 1;
    }

    if (rfs->min > rfs->max) {
        LOG_ERR("Minimum size can't be greater than maximum size.");
        return 1;
    }

    return 0;
}

static int parse_home_row_keys(void *dest, char *value) {
    char ***home_row_keys_ptr = dest;

    if (value[0] == '\0') {
        *home_row_keys_ptr = NULL;
        return 0;
    }

#define ASSERT_FOLLOW_UP_BYTE(c)           \
    if ((*c & 0b11000000) != 0b10000000) { \
        LOG_ERR("Encoding error.");        \
        goto err;                          \
    }

    char  *b    = malloc(HOME_ROW_LEN_WITH_BTN * 5);
    char **keys = malloc(HOME_ROW_LEN_WITH_BTN * sizeof(char *));

    char *c = value;

    for (int i = 0; i < HOME_ROW_LEN_WITH_BTN; i++) {
        if (*c == 0) {
            LOG_ERR("Could not parse home row keys. Not enough characters.");
            LOG_ERR(
                "Exactly 11 are required. The first 8 are used to select "
                "the sub-areas in the bisect mode."
            );
            LOG_ERR(
                "The next three characters are used for left, right and "
                "middle click respectively."
            );
            goto err;
        }

        if ((*c & 0b10000000) == 0) {
            b[i * 5]     = *c++;
            b[i * 5 + 1] = 0;
        } else if ((*c & 0b11100000) == 0b11000000) {
            b[i * 5] = *c++;
            ASSERT_FOLLOW_UP_BYTE(c);
            b[i * 5 + 1] = *c++;
            b[i * 5 + 2] = 0;
        } else if ((*c & 0b11110000) == 0b11100000) {
            b[i * 5] = *c++;
            ASSERT_FOLLOW_UP_BYTE(c);
            b[i * 5 + 1] = *c++;
            ASSERT_FOLLOW_UP_BYTE(c);
            b[i * 5 + 2] = *c++;
            b[i * 5 + 3] = 0;
        } else if ((*c & 0b11111000) == 0b11110000) {
            b[i * 5] = *c++;
            ASSERT_FOLLOW_UP_BYTE(c);
            b[i * 5 + 1] = *c++;
            ASSERT_FOLLOW_UP_BYTE(c);
            b[i * 5 + 2] = *c++;
            ASSERT_FOLLOW_UP_BYTE(c);
            b[i * 5 + 3] = *c++;
            b[i * 5 + 4] = 0;
        }

        keys[i] = &b[i * 5];
    }

    if (*c != '\0') {
        LOG_ERR("Too many characters. Exactly 11 are required.");
        goto err;
    }

    *home_row_keys_ptr = keys;

    return 0;

err:
    free(b);
    free(keys);
    return 1;
}

static int parse_str(void *dest, char *value) {
    *((char **)dest) = strdup(value);
    return 0;
}

static int parse_floating_mode_source_value(void *dest, char *value) {
    enum floating_mode_source *out = dest;
    if (strcmp(value, "stdin") == 0) {
        *out = FLOATING_MODE_SOURCE_STDIN;
    } else if (strcmp(value, "detect") == 0) {
#if OPENCV_ENABLED
        *out = FLOATING_MODE_SOURCE_DETECT;
#else
        LOG_ERR("Binary not build with OpenCV. 'detect' source not supported.");
        return 2;
#endif
    } else {
        LOG_ERR("Invalid source '%s'. Should be 'stdin' or 'detect'.", value);
        return 1;
    }

    return 0;
}

static int parse_click(void *dest, char *value) {
    enum click *out = dest;
    if (strcmp(value, "none") == 0) {
        *out = CLICK_NONE;
    } else if (strcmp(value, "left") == 0) {
        *out = CLICK_LEFT_BTN;
    } else if (strcmp(value, "right") == 0) {
        *out = CLICK_RIGHT_BTN;
    } else if (strcmp(value, "middle") == 0) {
        *out = CLICK_MIDDLE_BTN;
    } else {
        LOG_ERR(
            "Invalid click button '%s'. Should be 'left', 'right', 'middle' or "
            "'none'.",
            value
        );
        return 1;
    }

    return 0;
}

static void free_home_row_keys(void *field_value) {
    char ***home_row_keys_ptr = field_value;
    if (*home_row_keys_ptr == NULL) {
        return;
    }

    free(**home_row_keys_ptr);
    free(*home_row_keys_ptr);

    *home_row_keys_ptr = NULL;
}

static void free_str(void *field_value) {
    free(*((char **)field_value));
}

struct field_def {
    char  *name;
    size_t offset;
    char  *default_value;
    int (*parse)(void *dest, char *value);
    void (*free)(void *value);
};

struct section_def {
    char              *name;
    size_t             offset;
    struct field_def **fields;
};

#define SECTION(name, ...)                                             \
    (struct section_def) {                                             \
        #name, offsetof(struct config, name), (struct field_def *[]) { \
            __VA_ARGS__, NULL                                          \
        }                                                              \
    }

#define FIELD(type, name, default_value, parse, free)           \
    (struct field_def[]) {                                      \
        #name, offsetof(type, name), default_value, parse, free \
    }

#define G_FIELD(name, default_value, parse, free) \
    FIELD(struct general_config, name, default_value, parse, free)
#define MT_FIELD(name, default_value, parse, free) \
    FIELD(struct mode_tile_config, name, default_value, parse, free)
#define MF_FIELD(name, default_value, parse, free) \
    FIELD(struct mode_floating_config, name, default_value, parse, free)
#define MB_FIELD(name, default_value, parse, free) \
    FIELD(struct mode_bisect_config, name, default_value, parse, free)
#define MS_FIELD(name, default_value, parse, free) \
    FIELD(struct mode_split_config, name, default_value, parse, free)
#define MC_FIELD(name, default_value, parse, free) \
    FIELD(struct mode_click_config, name, default_value, parse, free)

static void noop() {}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
static struct section_def section_defs[] = {
    SECTION(
        general,
        G_FIELD(home_row_keys, "", parse_home_row_keys, free_home_row_keys),
        G_FIELD(modes, "tile,bisect", parse_str, free_str),
        G_FIELD(cancellation_status_code, "0", parse_uint8, noop)
    ),
    SECTION(
        mode_tile, MT_FIELD(label_color, "#fffd", parse_color, noop),
        MT_FIELD(label_select_color, "#fd0d", parse_color, noop),
        MT_FIELD(unselectable_bg_color, "#2226", parse_color, noop),
        MT_FIELD(selectable_bg_color, "#0304", parse_color, noop),
        MT_FIELD(selectable_border_color, "#040c", parse_color, noop),
        MT_FIELD(label_font_family, "sans-serif", parse_str, free_str),
        MT_FIELD(label_font_size, "8 50% 100", parse_relative_font_size, noop),
        MT_FIELD(
            label_symbols, "abcdefghijklmnopqrstuvwxyz", parse_str, free_str
        ),
        MT_FIELD(keys, "", parse_str, free_str)
    ),
    SECTION(
        mode_floating,
        MF_FIELD(source, "stdin", parse_floating_mode_source_value, noop),
        MF_FIELD(label_color, "#fffd", parse_color, noop),
        MF_FIELD(label_select_color, "#fd0d", parse_color, noop),
        MF_FIELD(unselectable_bg_color, "#2226", parse_color, noop),
        MF_FIELD(selectable_bg_color, "#1718", parse_color, noop),
        MF_FIELD(selectable_border_color, "#040c", parse_color, noop),
        MF_FIELD(label_font_family, "sans-serif", parse_str, free_str),
        MF_FIELD(label_font_size, "12 50% 100", parse_relative_font_size, noop),
        MF_FIELD(
            label_symbols, "abcdefghijklmnopqrstuvwxyz", parse_str, free_str
        ),
        MF_FIELD(keys, "", parse_str, free_str)
    ),
    SECTION(
        mode_bisect, MB_FIELD(label_color, "#fffd", parse_color, noop),
        // TODO: we should set minimums for numbers.
        MB_FIELD(label_font_size, "20", parse_double, noop),
        MB_FIELD(label_font_family, "sans-serif", parse_str, free_str),
        MB_FIELD(label_padding, "12", parse_double, noop),
        MB_FIELD(pointer_size, "20", parse_double, noop),
        MB_FIELD(pointer_color, "#e22d", parse_color, noop),
        MB_FIELD(unselectable_bg_color, "#2226", parse_color, noop),
        MB_FIELD(even_area_bg_color, "#0304", parse_color, noop),
        MB_FIELD(even_area_border_color, "#0408", parse_color, noop),
        MB_FIELD(odd_area_bg_color, "#0034", parse_color, noop),
        MB_FIELD(odd_area_border_color, "#0048", parse_color, noop),
        MB_FIELD(history_border_color, "#3339", parse_color, noop)
    ),
    SECTION(
        mode_split, MS_FIELD(pointer_size, "20", parse_double, noop),
        MS_FIELD(pointer_color, "#e22d", parse_color, noop),
        MS_FIELD(bg_color, "#2226", parse_color, noop),
        MS_FIELD(area_bg_color, "#11111188", parse_color, noop),
        MS_FIELD(vertical_color, "#8888ffcc", parse_color, noop),
        MS_FIELD(horizontal_color, "#008800cc", parse_color, noop),
        MS_FIELD(history_border_color, "#3339", parse_color, noop)
    ),
    SECTION(mode_click, MC_FIELD(button, "left", parse_click, noop)),
};
#pragma GCC diagnostic pop

void print_default_config() {
    puts("# wl-kbptr can be configured with a configuration file.");
    puts("# The file location can be passed with the -c parameter.");
    puts("# Othewise the `$XDG_CONFIG_HOME/wl-kbptr/config` file will");
    puts("# be loaded if it exits. Below is the default configuration.");

    for (int i = 0; i < sizeof(section_defs) / sizeof(section_defs[0]); i++) {
        struct section_def *section_def = &section_defs[i];

        printf("\n[%s]\n", section_def->name);

        for (struct field_def **field_def_ptr = section_def->fields;
             *field_def_ptr != NULL; field_def_ptr++) {
            struct field_def *field_def = *field_def_ptr;

            printf("%s=%s\n", field_def->name, field_def->default_value);
        }
    }
}

void config_loader_init(struct config_loader *loader, struct config *config) {
    loader->config           = config;
    loader->curr_section_def = &section_defs[0];
}

int config_loader_enter_section(struct config_loader *loader, char *section) {
    for (int i = 0; i < sizeof(section_defs) / sizeof(section_defs[0]); i++) {
        struct section_def *section_def = &section_defs[i];

        if (strcmp(section, section_def->name) == 0) {
            loader->curr_section_def = section_def;
            return 0;
        }
    }

    LOG_ERR("Configuration section `%s` does not exist.", section);
    return 1;
}

int config_loader_load_field(
    struct config_loader *loader, char *name, char *value
) {
    struct section_def *section_def = loader->curr_section_def;

    for (struct field_def **field_defs = section_def->fields;
         *field_defs != NULL; field_defs++) {
        struct field_def *field_def = *field_defs;

        if (strcmp(name, field_def->name) == 0) {
            void *dest = ((void *)loader->config) + section_def->offset +
                         field_def->offset;
            field_def->free(dest);
            int err = field_def->parse(dest, value);
            if (err != 0) {
                LOG_ERR("Invalid value for %s.%s.", section_def->name, name);
                return 2;
            }

            return 0;
        }
    }

    LOG_ERR(
        "Configuration option `%s.%s` does not exist.", section_def->name, name
    );
    return 1;
}

int config_loader_load_cli_param(struct config_loader *loader, char *value) {
    // `buf` will hold a copy of the section and field name as
    // `section\0field\0`.
    char buf[strlen(value)];

    // `bc` holds the current position in `buf`. Each time we write to `buf`
    // this pointer is incremented.
    char *bc = buf;

    // `c` holds the current position in `value`.
    char *c = value;

    char *section = buf;
    while (*c != '\0' && *c != '.' && *c != '=') {
        *(bc++) = *(c++);
    }

    char *field_name;

    if (*c == '.') {
        *(bc++) = '\0';
        c++;

        field_name = bc;
        while (*c != '\0' && *c != '=') {
            *(bc++) = *(c++);
        }
    } else {
        field_name = section;
        section    = "general";
    }

    if (*c != '=') {
        LOG_ERR("Invalid configuration parameter `%s`.", value);
        return 1;
    }

    *(bc++) = '\0';
    c++;

    int err = config_loader_enter_section(loader, section);
    if (err != 0) {
        return 2;
    }

    char *field_value = c;
    err = config_loader_load_field(loader, field_name, field_value);
    if (err != 0) {
        return 3;
    }

    return 0;
}

void config_set_default(struct config *config) {
    for (int i = 0; i < sizeof(section_defs) / sizeof(section_defs[0]); i++) {
        struct section_def *section_def = &section_defs[i];
        for (struct field_def **field_def_ptr = section_def->fields;
             *field_def_ptr != NULL; field_def_ptr++) {
            struct field_def *field_def = *field_def_ptr;

            int err = field_def->parse(
                ((void *)config) + section_def->offset + field_def->offset,
                field_def->default_value
            );
            if (err != 0) {
                LOG_ERR(
                    "Could not set default value '%s' for %s.%s.",
                    field_def->default_value, section_def->name, field_def->name
                );
            }
        }
    }
}

void config_free_values(struct config *config) {
    for (int i = 0; i < sizeof(section_defs) / sizeof(section_defs[0]); i++) {
        struct section_def *section_def = &section_defs[i];
        for (struct field_def **field_def_ptr = section_def->fields;
             *field_def_ptr != NULL; field_def_ptr++) {
            struct field_def *field_def = *field_def_ptr;

            field_def->free(
                ((void *)config) + section_def->offset + field_def->offset
            );
        }
    }
}

static const char *XDG_PATH_FMT = "%s/wl-kbptr/config";

static FILE *open_config_file(char *file_name) {
    FILE *f = NULL;

    if (file_name != NULL) {
        f = fopen(file_name, "r");
        if (f == NULL) {
            LOG_ERR("Could not open config file '%s'", file_name);
            return NULL;
        }

        LOG_INFO("Loading config file '%s'", file_name);
        return f;
    }

    char       *xdg_config_home     = getenv("XDG_CONFIG_HOME");
    const char *home                = getenv("HOME");
    size_t      config_home_buf_len = 0;

    if (xdg_config_home == NULL && home != NULL) {
        config_home_buf_len = strlen(home) + sizeof("/.config");
    }

    char config_home_buf[config_home_buf_len];

    if (xdg_config_home == NULL && home != NULL) {
        snprintf(config_home_buf, config_home_buf_len, "%s/.config", home);
        xdg_config_home = config_home_buf;
    }

    if (xdg_config_home != NULL) {
        int  path_len = snprintf(NULL, 0, XDG_PATH_FMT, xdg_config_home) + 1;
        char file_path[path_len];
        snprintf(file_path, path_len, XDG_PATH_FMT, xdg_config_home);

        f = fopen(file_path, "r");
        if (f == NULL) {
            LOG_WARN("Could not open config file '%s'", file_path);
            return NULL;
        }

        LOG_INFO("Loading config file '%s'", file_path);
    }

    return f;
}

static const char *WHITE_SPACES = "\r\t ";

int config_loader_load_file(struct config_loader *loader, char *file_name) {
    FILE *f = open_config_file(file_name);
    if (f == NULL) {
        return file_name == NULL ? 0 : 1;
    }

    char buf[256];
    int  c = 0, i, err;

    for (int line; c != EOF; line++) {
        do {
            c = getc(f);
        } while (strchr(WHITE_SPACES, c) != NULL);

        switch (c) {
        // Comment
        case '#':
            do {
                c = getc(f);
            } while (c != '\n' && c != EOF);
            break;

        // Section
        case '[':
            for (i = 0; i < (sizeof(buf) - 1) && (c = getc(f)) != ']'; i++) {
                if (c == EOF) {
                    LOG_ERR(
                        "Unexpected end of file. Section was not terminated."
                    );
                    goto err;
                } else if (c == '\n') {
                    LOG_ERR(
                        "Unexpected end of line. Section was not terminated."
                    );
                    goto err;
                }
                buf[i] = c;
            }

            if (c != ']') {
                LOG_ERR("Line is too long.");
                goto err;
            }

            buf[i] = '\0';

            do {
                c = getc(f);
                if (strchr(WHITE_SPACES, c) != NULL) {
                    LOG_ERR(
                        "Only whitespaces are allowed after a section's ending "
                        "delimiter."
                    );
                    goto err;
                }
            } while (c != '\n' && c != EOF);

            err = config_loader_enter_section(loader, buf) != 0;
            if (err) {
                goto err;
            }
            break;

        // Empty line
        case '\n':
            break;

        case EOF:
            goto success;

        // key=value
        default:
            buf[0] = c;
            for (i = 1; i < (sizeof(buf) - 1) && (c = getc(f)) != '='; i++) {
                if (c == EOF) {
                    LOG_ERR("Unexpected end of file. Expected equal operator.");
                    goto err;
                } else if (c == '\n') {
                    LOG_ERR("Unexpected end of line. Expected equal operator.");
                    goto err;
                }
                buf[i] = c;
            }

            if (c != '=') {
                LOG_ERR("Line is too long");
                goto err;
            }

            buf[i++] = '\0';

            char *value = &buf[i];
            for (; i < (sizeof(buf)); i++) {
                c = getc(f);
                if (c == EOF || c == '\n') {
                    break;
                } else if (c == '\r') {
                    continue;
                }

                buf[i] = c;
            }

            if (i == sizeof(buf)) {
                LOG_ERR("Line is too long");
            }

            buf[i] = '\0';

            err = config_loader_load_field(loader, buf, value);
            if (err) {
                goto err;
            }
        }
    }

success:
    fclose(f);
    return 0;

err:
    fclose(f);
    return 1;
}

double
compute_relative_font_size(struct relative_font_size *rfs, double height) {
    double value = height * rfs->proportion;
    if (value < rfs->min) {
        return rfs->min;
    }

    if (value > rfs->max) {
        return rfs->max;
    }

    return value;
}
