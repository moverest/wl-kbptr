#ifndef __CONFIG_H_INCLUDED__
#define __CONFIG_H_INCLUDED__

#include "utils.h"

#include <stdint.h>

struct general_config {
    char **home_row_keys;
    char  *modes;
};

struct mode_tile_config {
    uint32_t label_color;
    uint32_t label_select_color;
    uint32_t unselectable_bg_color;
    uint32_t selectable_bg_color;
    uint32_t selectable_border_color;
    char    *label_font_family;
    char    *label_symbols;
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

#endif
