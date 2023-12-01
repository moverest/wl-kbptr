#ifndef __SURFACE_CONFIG_H_INCLUDED__
#define __SURFACE_CONFIG_H_INCLUDED__

#include <stdint.h>
#include <stdio.h>

struct general_config {
    char **home_row_keys;
};

struct mode_tile_config {
    uint32_t label_color;
    uint32_t label_select_color;
    uint32_t unselectable_bg_color;
    uint32_t selectable_bg_color;
    uint32_t selectable_border_color;
};

struct mode_bisect_config {
    uint32_t label_color;
    double   label_font_size;
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

struct config {
    struct general_config     general;
    struct mode_tile_config   mode_tile;
    struct mode_bisect_config mode_bisect;
};

/**
 * The `config_loader` structure stores needed states to set parse values.
 */
struct config_loader {
    struct config *config;
    void          *curr_section_def;
};

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
