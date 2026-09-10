// SPDX-License-Identifier: GPL-3.0-only

#include "settings_overlay.h"

#include "log.h"
#include "mode.h"
#include "state.h"
#include "utils.h"
#include "utils_cairo.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EDITABLE_FIELDS 64
#define VALUE_BUF_LEN       64
#define MAX_VALUE_PARTS     8
#define PART_LEN            16

// ponytail: the panel's own colors are hardcoded. Making them configurable
// would mean editing the settings UI from within the settings UI.
#define PANEL_BG_COLOR     0x1a1a24f2
#define PANEL_BORDER_COLOR 0x4a4a5aff
#define PANEL_HEADER_COLOR 0x88c0d0ff
#define PANEL_NAME_COLOR   0x9a9aa8ff
#define PANEL_VALUE_COLOR  0xd0d0dcff
#define PANEL_SELECTED_BG  0x33334aff
#define PANEL_COMPONENT_HL 0xffd166ff
#define PANEL_DIRTY_COLOR  0xffd166ff
#define PANEL_STATUS_COLOR 0x9ece6aff
#define PANEL_HELP_COLOR   0x70707eff

#define PANEL_MARGIN    24.
#define PANEL_PADDING   12.
#define PANEL_WIDTH     470.
#define PANEL_FONT_SIZE 13.
#define PANEL_LINE      18.
#define PANEL_NAME_COL  250.
#define PANEL_SWATCH    11.

union field_value {
    uint32_t                  color;
    double                    number;
    struct relative_font_size rel_font_size;
};

struct settings_overlay {
    struct config_field_ref fields[MAX_EDITABLE_FIELDS];
    union field_value       originals[MAX_EDITABLE_FIELDS];
    int                     num_fields;

    int selected;
    int component;

    cairo_font_face_t *font_face;
    // Roomy enough to hold a full path plus a message without truncation.
    char               status[PATH_MAX + 128];
};

static int field_num_components(const struct config_field_ref *field) {
    switch (field->kind) {
    case CONFIG_FIELD_COLOR:
        return 4;
    case CONFIG_FIELD_REL_FONT_SIZE:
        return 3;
    default:
        return 1;
    }
}

static union field_value field_read(const struct config_field_ref *field) {
    // Zeroed in full rather than with an initializer: `field_is_dirty` compares
    // whole unions, so the bytes the smaller members leave over must be
    // deterministic.
    union field_value value;
    memset(&value, 0, sizeof(value));

    switch (field->kind) {
    case CONFIG_FIELD_COLOR:
        value.color = *(uint32_t *)field->value;
        break;
    case CONFIG_FIELD_DOUBLE:
        value.number = *(double *)field->value;
        break;
    case CONFIG_FIELD_REL_FONT_SIZE:
        value.rel_font_size = *(struct relative_font_size *)field->value;
        break;
    default:
        break;
    }

    return value;
}

static void
field_write(const struct config_field_ref *field, union field_value value) {
    switch (field->kind) {
    case CONFIG_FIELD_COLOR:
        *(uint32_t *)field->value = value.color;
        break;
    case CONFIG_FIELD_DOUBLE:
        *(double *)field->value = value.number;
        break;
    case CONFIG_FIELD_REL_FONT_SIZE:
        *(struct relative_font_size *)field->value = value.rel_font_size;
        break;
    default:
        break;
    }
}

static bool field_is_dirty(struct settings_overlay *ov, int i) {
    union field_value current = field_read(&ov->fields[i]);
    return memcmp(&current, &ov->originals[i], sizeof(current)) != 0;
}

static int section_first(struct settings_overlay *ov, int i) {
    while (i > 0 &&
           strcmp(ov->fields[i - 1].section, ov->fields[i].section) == 0) {
        i--;
    }
    return i;
}

static int section_last(struct settings_overlay *ov, int i) {
    while (i + 1 < ov->num_fields &&
           strcmp(ov->fields[i + 1].section, ov->fields[i].section) == 0) {
        i++;
    }
    return i;
}

int settings_overlay_init(struct state *state) {
    struct settings_overlay *ov = calloc(1, sizeof(*ov));
    if (ov == NULL) {
        LOG_ERR("Could not allocate the settings overlay.");
        return 1;
    }

    ov->num_fields =
        config_editable_fields(&state->config, ov->fields, MAX_EDITABLE_FIELDS);
    if (ov->num_fields == 0) {
        LOG_ERR("No editable configuration fields.");
        free(ov);
        return 1;
    }

    for (int i = 0; i < ov->num_fields; i++) {
        ov->originals[i] = field_read(&ov->fields[i]);
    }

    // Start on the section of the first mode in the chain, since that is what
    // is on screen and therefore the only section that previews live.
    if (state->mode_interfaces[0] != NULL) {
        char section[64];
        snprintf(
            section, sizeof(section), "mode_%s", state->mode_interfaces[0]->name
        );

        for (int i = 0; i < ov->num_fields; i++) {
            if (strcmp(ov->fields[i].section, section) == 0) {
                ov->selected = i;
                break;
            }
        }
    }

    ov->font_face = cairo_toy_font_face_create(
        "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );

    state->settings = ov;
    return 0;
}

void settings_overlay_free(struct state *state) {
    struct settings_overlay *ov = state->settings;
    if (ov == NULL) {
        return;
    }

    cairo_font_face_destroy(ov->font_face);
    free(ov);
    state->settings = NULL;
}

static void adjust_double(double *value, double step) {
    *value += step;
    if (*value < 0) {
        *value = 0;
    }
}

static void adjust_field(struct settings_overlay *ov, int step) {
    struct config_field_ref *field = &ov->fields[ov->selected];
    union field_value        value = field_read(field);

    switch (field->kind) {
    case CONFIG_FIELD_COLOR:;
        const int shift   = (3 - ov->component) * 8;
        int       channel = (value.color >> shift) & 0xff;

        channel += step;
        if (channel < 0) {
            channel = 0;
        } else if (channel > 0xff) {
            channel = 0xff;
        }

        value.color =
            (value.color & ~(0xffu << shift)) | ((uint32_t)channel << shift);
        break;

    case CONFIG_FIELD_DOUBLE:
        adjust_double(&value.number, step);
        break;

    case CONFIG_FIELD_REL_FONT_SIZE:
        switch (ov->component) {
        case 0:
            adjust_double(&value.rel_font_size.min, step);
            break;
        case 1:
            // The proportion is shown as a percentage, so a step is a point.
            adjust_double(&value.rel_font_size.proportion, step / 100.);
            break;
        default:
            adjust_double(&value.rel_font_size.max, step);
            break;
        }
        break;

    default:
        return;
    }

    field_write(field, value);
    ov->status[0] = '\0';
}

static void select_field(struct settings_overlay *ov, int index) {
    if (index < 0 || index >= ov->num_fields) {
        return;
    }

    ov->selected = index;

    const int num_components = field_num_components(&ov->fields[index]);
    if (ov->component >= num_components) {
        ov->component = num_components - 1;
    }
}

bool settings_overlay_handle_key(
    struct state *state, xkb_keysym_t keysym, char *text
) {
    struct settings_overlay *ov = state->settings;

    const int coarse =
        ov->fields[ov->selected].kind == CONFIG_FIELD_COLOR ? 16 : 10;

    switch (keysym) {
    case XKB_KEY_Escape:
    case XKB_KEY_q:
        state->settings_open = false;
        return true;

    case XKB_KEY_Up:
    case XKB_KEY_k:
        if (ov->selected > section_first(ov, ov->selected)) {
            select_field(ov, ov->selected - 1);
            return true;
        }
        return false;

    case XKB_KEY_Down:
    case XKB_KEY_j:
        if (ov->selected < section_last(ov, ov->selected)) {
            select_field(ov, ov->selected + 1);
            return true;
        }
        return false;

    case XKB_KEY_Left:
    case XKB_KEY_h:
        adjust_field(ov, -1);
        return true;

    case XKB_KEY_Right:
    case XKB_KEY_l:
        adjust_field(ov, 1);
        return true;

    case XKB_KEY_H:
        adjust_field(ov, -coarse);
        return true;

    case XKB_KEY_L:
        adjust_field(ov, coarse);
        return true;

    case XKB_KEY_Tab:
        ov->component = (ov->component + 1) %
                        field_num_components(&ov->fields[ov->selected]);
        return true;

    case XKB_KEY_ISO_Left_Tab:;
        const int num_components =
            field_num_components(&ov->fields[ov->selected]);
        ov->component = (ov->component + num_components - 1) % num_components;
        return true;

    case XKB_KEY_bracketleft:;
        const int first = section_first(ov, ov->selected);
        if (first > 0) {
            select_field(ov, section_first(ov, first - 1));
            return true;
        }
        return false;

    case XKB_KEY_bracketright:;
        const int next = section_last(ov, ov->selected) + 1;
        if (next < ov->num_fields) {
            select_field(ov, next);
            return true;
        }
        return false;

    case XKB_KEY_r:
        field_write(&ov->fields[ov->selected], ov->originals[ov->selected]);
        ov->status[0] = '\0';
        return true;

    case XKB_KEY_s:
        settings_overlay_save(state);
        return true;

    default:
        return false;
    }
}

void settings_overlay_patch_config(
    FILE *in, FILE *out, const struct config_field_ref *changes,
    const char **values, int num_changes
) {
    bool handled[num_changes > 0 ? num_changes : 1];
    memset(handled, 0, sizeof(handled));

    char section[128] = "";
    char line[512];

    while (in != NULL && fgets(line, sizeof(line), in) != NULL) {
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end != NULL) {
                size_t len = end - p - 1;
                if (len >= sizeof(section)) {
                    len = sizeof(section) - 1;
                }
                memcpy(section, p + 1, len);
                section[len] = '\0';
            }

            fputs(line, out);
            continue;
        }

        char *equal = strchr(p, '=');
        if (*p == '#' || *p == ';' || equal == NULL) {
            fputs(line, out);
            continue;
        }

        char   key[128];
        size_t key_len = equal - p;
        while (key_len > 0 &&
               (p[key_len - 1] == ' ' || p[key_len - 1] == '\t')) {
            key_len--;
        }
        if (key_len >= sizeof(key)) {
            key_len = sizeof(key) - 1;
        }
        memcpy(key, p, key_len);
        key[key_len] = '\0';

        int match = -1;
        for (int i = 0; i < num_changes; i++) {
            if (!handled[i] && strcmp(changes[i].section, section) == 0 &&
                strcmp(changes[i].name, key) == 0) {
                match = i;
                break;
            }
        }

        if (match < 0) {
            fputs(line, out);
            continue;
        }

        fprintf(out, "%s=%s\n", key, values[match]);
        handled[match] = true;
    }

    // Whatever was not already in the file gets appended. The loader is
    // sequential and later values overwrite earlier ones, so repeating a
    // section at the end is enough and avoids splicing into the middle.
    const char *last_section = NULL;
    for (int i = 0; i < num_changes; i++) {
        if (handled[i]) {
            continue;
        }

        if (last_section == NULL ||
            strcmp(last_section, changes[i].section) != 0) {
            fprintf(out, "\n[%s]\n", changes[i].section);
            last_section = changes[i].section;
        }

        fprintf(out, "%s=%s\n", changes[i].name, values[i]);
    }
}

int settings_overlay_save(struct state *state) {
    struct settings_overlay *ov = state->settings;

    struct config_field_ref changes[MAX_EDITABLE_FIELDS];
    char                    value_bufs[MAX_EDITABLE_FIELDS][VALUE_BUF_LEN];
    const char             *values[MAX_EDITABLE_FIELDS];
    int                     dirty[MAX_EDITABLE_FIELDS];
    int                     num_changes = 0;

    for (int i = 0; i < ov->num_fields; i++) {
        if (!field_is_dirty(ov, i)) {
            continue;
        }

        changes[num_changes] = ov->fields[i];
        config_format_field(
            &ov->fields[i], value_bufs[num_changes], VALUE_BUF_LEN
        );
        values[num_changes] = value_bufs[num_changes];
        dirty[num_changes]  = i;
        num_changes++;
    }

    if (num_changes == 0) {
        snprintf(ov->status, sizeof(ov->status), "No changes to save.");
        return 0;
    }

    char path[PATH_MAX];
    if (config_resolve_path(state->config_filename, path, sizeof(path)) != 0) {
        snprintf(ov->status, sizeof(ov->status), "No config path to save to.");
        return 1;
    }

    char *slash = strrchr(path, '/');
    if (slash != NULL && slash != path) {
        *slash = '\0';
        if (mkdir(path, 0755) != 0 && errno != EEXIST) {
            snprintf(
                ov->status, sizeof(ov->status), "Could not create %s: %s", path,
                strerror(errno)
            );
            *slash = '/';
            return 1;
        }
        *slash = '/';
    }

    char tmp_path[PATH_MAX];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.XXXXXX", path) >=
        (int)sizeof(tmp_path)) {
        snprintf(ov->status, sizeof(ov->status), "Config path is too long.");
        return 1;
    }

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        snprintf(
            ov->status, sizeof(ov->status), "Could not write near %s: %s", path,
            strerror(errno)
        );
        return 1;
    }

    struct stat st;
    if (fchmod(fd, stat(path, &st) == 0 ? st.st_mode & 07777 : 0644) != 0) {
        LOG_WARN("Could not set the config file mode.");
    }

    FILE *out = fdopen(fd, "w");
    if (out == NULL) {
        snprintf(ov->status, sizeof(ov->status), "Could not write the config.");
        close(fd);
        unlink(tmp_path);
        return 1;
    }

    FILE *in = fopen(path, "r");
    settings_overlay_patch_config(in, out, changes, values, num_changes);
    if (in != NULL) {
        fclose(in);
    }

    if (fclose(out) != 0) {
        snprintf(
            ov->status, sizeof(ov->status), "Could not write the config: %s",
            strerror(errno)
        );
        unlink(tmp_path);
        return 1;
    }

    if (rename(tmp_path, path) != 0) {
        snprintf(
            ov->status, sizeof(ov->status), "Could not replace %s: %s", path,
            strerror(errno)
        );
        unlink(tmp_path);
        return 1;
    }

    // The saved values are the new baseline.
    for (int i = 0; i < num_changes; i++) {
        ov->originals[dirty[i]] = field_read(&ov->fields[dirty[i]]);
    }

    LOG_INFO("Saved the configuration to '%s'", path);
    snprintf(
        ov->status, sizeof(ov->status), "Saved %d field%s.", num_changes,
        num_changes == 1 ? "" : "s"
    );
    return 0;
}

/**
 * `value_parts` splits a field's value into display chunks so the selected
 * component can be highlighted. `component_of[i]` is the component a chunk
 * belongs to, or -1 for separators. Returns the number of chunks.
 */
static int value_parts(
    const struct config_field_ref *field, char parts[][PART_LEN],
    int component_of[]
) {
    switch (field->kind) {
    case CONFIG_FIELD_COLOR:;
        const uint32_t color = *(uint32_t *)field->value;

        snprintf(parts[0], PART_LEN, "#");
        component_of[0] = -1;

        for (int i = 0; i < 4; i++) {
            snprintf(
                parts[i + 1], PART_LEN, "%02x", (color >> ((3 - i) * 8)) & 0xff
            );
            component_of[i + 1] = i;
        }

        return 5;

    case CONFIG_FIELD_REL_FONT_SIZE:;
        struct relative_font_size *rfs = field->value;

        snprintf(parts[0], PART_LEN, "%g", rfs->min);
        component_of[0] = 0;
        snprintf(parts[1], PART_LEN, " ");
        component_of[1] = -1;
        snprintf(parts[2], PART_LEN, "%g%%", rfs->proportion * 100.);
        component_of[2] = 1;
        snprintf(parts[3], PART_LEN, " ");
        component_of[3] = -1;
        snprintf(parts[4], PART_LEN, "%g", rfs->max);
        component_of[4] = 2;

        return 5;

    default:
        config_format_field(field, parts[0], PART_LEN);
        component_of[0] = 0;
        return 1;
    }
}

static void draw_text(
    cairo_t *cairo, double x, double y, uint32_t color, const char *text
) {
    cairo_set_source_u32(cairo, color);
    cairo_move_to(cairo, x, y);
    cairo_show_text(cairo, text);
}

void settings_overlay_render(struct state *state, cairo_t *cairo) {
    if (!state->settings_open || state->settings == NULL) {
        return;
    }

    struct settings_overlay *ov = state->settings;

    const int first = section_first(ov, ov->selected);
    const int last  = section_last(ov, ov->selected);
    const int rows  = last - first + 1;

    // Header, the fields, a blank line, two help lines and the status line.
    const double height = PANEL_PADDING * 2 + PANEL_LINE * (rows + 5);

    cairo_save(cairo);
    cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
    cairo_set_font_face(cairo, ov->font_face);
    cairo_set_font_size(cairo, PANEL_FONT_SIZE);

    cairo_translate(cairo, PANEL_MARGIN, PANEL_MARGIN);

    cairo_set_source_u32(cairo, PANEL_BG_COLOR);
    cairo_rectangle(cairo, 0, 0, PANEL_WIDTH, height);
    cairo_fill(cairo);

    cairo_set_source_u32(cairo, PANEL_BORDER_COLOR);
    cairo_rectangle(cairo, .5, .5, PANEL_WIDTH - 1, height - 1);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    // Keeps long values and error messages inside the panel.
    cairo_rectangle(cairo, 1, 1, PANEL_WIDTH - 2, height - 2);
    cairo_clip(cairo);

    double y = PANEL_PADDING + PANEL_LINE;

    char header[128];
    snprintf(
        header, sizeof(header), "[%s]    [ ] to change section",
        ov->fields[first].section
    );
    draw_text(cairo, PANEL_PADDING, y, PANEL_HEADER_COLOR, header);
    y += PANEL_LINE;

    for (int i = first; i <= last; i++) {
        const bool selected = i == ov->selected;

        if (selected) {
            cairo_set_source_u32(cairo, PANEL_SELECTED_BG);
            cairo_rectangle(
                cairo, PANEL_PADDING - 4, y - PANEL_FONT_SIZE,
                PANEL_WIDTH - 2 * PANEL_PADDING + 8, PANEL_LINE
            );
            cairo_fill(cairo);
        }

        if (field_is_dirty(ov, i)) {
            draw_text(cairo, PANEL_PADDING - 2, y, PANEL_DIRTY_COLOR, "*");
        }

        draw_text(
            cairo, PANEL_PADDING + 14, y,
            selected ? PANEL_VALUE_COLOR : PANEL_NAME_COLOR, ov->fields[i].name
        );

        double x = PANEL_NAME_COL;

        if (ov->fields[i].kind == CONFIG_FIELD_COLOR) {
            cairo_set_source_u32(cairo, *(uint32_t *)ov->fields[i].value);
            cairo_rectangle(
                cairo, x, y - PANEL_SWATCH, PANEL_SWATCH, PANEL_SWATCH
            );
            cairo_fill(cairo);

            cairo_set_source_u32(cairo, PANEL_BORDER_COLOR);
            cairo_rectangle(
                cairo, x + .5, y - PANEL_SWATCH + .5, PANEL_SWATCH - 1,
                PANEL_SWATCH - 1
            );
            cairo_stroke(cairo);

            x += PANEL_SWATCH + 8;
        }

        char      parts[MAX_VALUE_PARTS][PART_LEN];
        int       component_of[MAX_VALUE_PARTS];
        const int num_parts = value_parts(&ov->fields[i], parts, component_of);

        for (int p = 0; p < num_parts; p++) {
            const bool highlighted =
                selected && component_of[p] == ov->component;

            draw_text(
                cairo, x, y,
                highlighted ? PANEL_COMPONENT_HL : PANEL_VALUE_COLOR, parts[p]
            );

            cairo_text_extents_t te;
            cairo_text_extents(cairo, parts[p], &te);
            x += te.x_advance;
        }

        y += PANEL_LINE;
    }

    y += PANEL_LINE;
    draw_text(
        cairo, PANEL_PADDING, y, PANEL_HELP_COLOR,
        "j/k field   h/l adjust   H/L faster   Tab part"
    );
    y += PANEL_LINE;
    draw_text(
        cairo, PANEL_PADDING, y, PANEL_HELP_COLOR,
        "s save   r reset field   q close"
    );
    y += PANEL_LINE;

    if (ov->status[0] != '\0') {
        draw_text(cairo, PANEL_PADDING, y, PANEL_STATUS_COLOR, ov->status);
    }

    cairo_restore(cairo);
}
