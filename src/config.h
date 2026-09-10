// SPDX-License-Identifier: GPL-3.0-only

#ifndef __CONFIG_H_INCLUDED__
#define __CONFIG_H_INCLUDED__

#include "utils.h"

#include <stddef.h>
#include <stdint.h>

struct general_config {
    char  **home_row_keys;
    char   *modes;
    uint8_t cancellation_status_code;
};

struct relative_font_size {
    double proportion;
    double min;
    double max;
};

struct mode_tile_config {
    uint32_t                  label_color;
    uint32_t                  label_select_color;
    uint32_t                  unselectable_bg_color;
    uint32_t                  selectable_bg_color;
    uint32_t                  selectable_border_color;
    char                     *label_font_family;
    struct relative_font_size label_font_size;
    char                     *label_symbols;
};

enum floating_mode_source {
    FLOATING_MODE_SOURCE_STDIN,
    FLOATING_MODE_SOURCE_DETECT,
};

struct mode_floating_config {
    enum floating_mode_source source;
    uint32_t                  label_color;
    uint32_t                  label_select_color;
    uint32_t                  unselectable_bg_color;
    uint32_t                  selectable_bg_color;
    uint32_t                  selectable_border_color;
    char                     *label_font_family;
    struct relative_font_size label_font_size;
    char                     *label_symbols;
};

struct mode_bisect_config {
    uint32_t label_color;
    double   label_font_size;
    char    *label_font_family;
    double   label_padding;

    double  pointer_size;
    int32_t pointer_color;

    uint32_t unselectable_bg_color;
    uint32_t even_area_bg_color;
    uint32_t even_area_border_color;
    uint32_t odd_area_bg_color;
    uint32_t odd_area_border_color;

    uint32_t history_border_color;
};

struct mode_split_config {
    double  pointer_size;
    int32_t pointer_color;

    uint32_t bg_color;
    uint32_t area_bg_color;
    uint32_t vertical_color;
    uint32_t horizontal_color;

    uint32_t history_border_color;
};

struct mode_click_config {
    enum click button;
};

struct config {
    struct general_config       general;
    struct mode_tile_config     mode_tile;
    struct mode_floating_config mode_floating;
    struct mode_bisect_config   mode_bisect;
    struct mode_split_config    mode_split;
    struct mode_click_config    mode_click;
};

/**
 * The `config_loader` structure stores needed states to set parse values.
 */
struct config_loader {
    struct config *config;
    void          *curr_section_def;
};

void print_default_config();

/**
 * `config_field_kind` is the type of a configuration field's value. It is
 * derived from the field's parse function, so no per-field annotation is
 * needed.
 */
enum config_field_kind {
    CONFIG_FIELD_OTHER = 0,
    CONFIG_FIELD_COLOR,
    CONFIG_FIELD_DOUBLE,
    CONFIG_FIELD_REL_FONT_SIZE,
};

/**
 * `config_field_ref` points at one field inside a live `struct config`.
 */
struct config_field_ref {
    const char            *section;
    const char            *name;
    enum config_field_kind kind;
    void                  *value;
};

/**
 * `config_editable_fields` fills `out` with every field whose value can be
 * edited numerically (colors and sizes). Returns the number written, which is
 * never greater than `max`.
 */
int config_editable_fields(
    struct config *config, struct config_field_ref *out, int max
);

/**
 * `config_format_field` writes the field's current value in the same syntax
 * its parse function accepts. Returns the number of characters that would have
 * been written, as `snprintf` does.
 */
int config_format_field(
    const struct config_field_ref *field, char *out, size_t out_len
);

/**
 * `config_resolve_path` writes the path of the configuration file that would
 * be loaded for `file_name` (which may be NULL to use the default locations)
 * into `out`. Returns 0 on success and non-zero if no path could be resolved.
 */
int config_resolve_path(const char *file_name, char *out, size_t out_len);

/**
 * `config_set_default` sets default values set in the configuration's
 * definitions.
 */
void config_set_default(struct config *config);

/**
 * `config_free_values` frees configuration fields' values. This doesn't free
 * the `config` structure itself.
 */
void config_free_values(struct config *config);

/**
 * `config_loader_init` initialise the `config_loader` structure.
 */
void config_loader_init(
    struct config_loader *config_loader, struct config *config
);

/**
 * `config_loader_enter_section` sets the current section's state. Following
 * calls to `config_loader_load_field` will load fields for the given section.
 */
int config_loader_enter_section(struct config_loader *loader, char *section);

/**
 * `config_loader_load_field` loads the give field value from the current
 * section.
 */
int config_loader_load_field(
    struct config_loader *loader, char *name, char *value
);

/**
 * `config_loader_load_cli_param` loads a configuration value from a CLI
 * parameter value, e.g. `mode_bisect.label_color=#66666666`.
 */
int config_loader_load_cli_param(struct config_loader *loader, char *value);

/**
 * `config_loader_load_file` loads configuration values from the given file or
 * from one of the default locations (if `file_name` is NULL).
 */
int config_loader_load_file(struct config_loader *loader, char *file_name);

double
compute_relative_font_size(struct relative_font_size *rfs, double height);

#endif
