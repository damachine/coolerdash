/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief INI parser handler for CoolerDash configuration.
 * @details Parses the configuration file and sets values in the Config struct.
 * @example
 *     See function documentation for usage examples.
 */

// Include project headers
#include "../include/config.h"
 
// Include necessary headers
#include <ini.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief INI parser handler, sets values in Config struct.
 * @details Called for each key-value pair in the INI file. Matches section and key names and sets the corresponding field in the Config struct. Unrecognized keys are ignored. For string fields, strncpy is used and buffers are always null-terminated for safety.
 * @example
 *     ini_parse("/opt/coolerdash/config.ini", inih_config_handler, &cfg);
 */
static int inih_config_handler(void *user, const char *section, const char *name, const char *value)
{
    Config *config = (Config *)user;

    // Helper function for safe string copying
    #define SAFE_STRCPY(dest, src) do { \
        strncpy(dest, src, sizeof(dest) - 1); \
        dest[sizeof(dest) - 1] = '\0'; \
    } while(0)

    // Helper function for color parsing
    #define PARSE_COLOR(color_struct, component) \
        if (strcmp(name, #component) == 0) color_struct.component = atoi(value)

    // Daemon section
    if (strcmp(section, "daemon") == 0) {
        if (strcmp(name, "address") == 0) {
            SAFE_STRCPY(config->daemon_address, value);
        } else if (strcmp(name, "password") == 0) {
            SAFE_STRCPY(config->daemon_password, value);
        }
    }
    // Paths section
    else if (strcmp(section, "paths") == 0) {
        if (strcmp(name, "images") == 0) {
            SAFE_STRCPY(config->paths_images, value);
        } else if (strcmp(name, "image_coolerdash") == 0) {
            SAFE_STRCPY(config->paths_image_coolerdash, value);
        } else if (strcmp(name, "image_shutdown") == 0) {
            SAFE_STRCPY(config->paths_image_shutdown, value);
        } else if (strcmp(name, "pid") == 0) {
            SAFE_STRCPY(config->paths_pid, value);
        }
    }
    // Display section
    else if (strcmp(section, "display") == 0) {
        if (strcmp(name, "width") == 0) config->display_width = atoi(value);
        else if (strcmp(name, "height") == 0) config->display_height = atoi(value);
        else if (strcmp(name, "refresh_interval_sec") == 0) config->display_refresh_interval_sec = atoi(value);
        else if (strcmp(name, "refresh_interval_nsec") == 0) config->display_refresh_interval_nsec = atoi(value);
        else if (strcmp(name, "brightness") == 0) config->lcd_brightness = atoi(value);
        else if (strcmp(name, "orientation") == 0) config->lcd_orientation = atoi(value);
        else if (strcmp(name, "temp_1_update_threshold") == 0) config->temp_1_update_threshold = (float)atof(value);
        else if (strcmp(name, "temp_2_update_threshold") == 0) config->temp_2_update_threshold = (float)atof(value);
    }
    // Layout section
    else if (strcmp(section, "layout") == 0) {
        if (strcmp(name, "box_width") == 0) config->layout_box_width = atoi(value);
        else if (strcmp(name, "box_height") == 0) config->layout_box_height = atoi(value);
        else if (strcmp(name, "box_gap") == 0) config->layout_box_gap = atoi(value);
        else if (strcmp(name, "bar_width") == 0) config->layout_bar_width = atoi(value);
        else if (strcmp(name, "bar_height") == 0) config->layout_bar_height = atoi(value);
        else if (strcmp(name, "bar_gap") == 0) config->layout_bar_gap = atoi(value);
        else if (strcmp(name, "bar_border_width") == 0) config->layout_bar_border_width = (float)atof(value);
    }
    // Color sections
    else if (strcmp(section, "bar_color_background") == 0) {
        PARSE_COLOR(config->layout_bar_color_background, r);
        else PARSE_COLOR(config->layout_bar_color_background, g);
        else PARSE_COLOR(config->layout_bar_color_background, b);
    }
    else if (strcmp(section, "bar_color_border") == 0) {
        PARSE_COLOR(config->layout_bar_color_border, r);
        else PARSE_COLOR(config->layout_bar_color_border, g);
        else PARSE_COLOR(config->layout_bar_color_border, b);
    }
    // Font section
    else if (strcmp(section, "font") == 0) {
        if (strcmp(name, "font_face") == 0) {
            SAFE_STRCPY(config->font_face, value);
        } else if (strcmp(name, "font_size_temp") == 0) config->font_size_temp = (float)atof(value);
        else if (strcmp(name, "font_size_labels") == 0) config->font_size_labels = (float)atof(value);
    }
    else if (strcmp(section, "font_color_temp") == 0) {
        PARSE_COLOR(config->font_color_temp, r);
        else PARSE_COLOR(config->font_color_temp, g);
        else PARSE_COLOR(config->font_color_temp, b);
    }
    else if (strcmp(section, "font_color_label") == 0) {
        PARSE_COLOR(config->font_color_label, r);
        else PARSE_COLOR(config->font_color_label, g);
        else PARSE_COLOR(config->font_color_label, b);
    }
    // Temperature section
    else if (strcmp(section, "temperature") == 0) {
        if (strcmp(name, "temp_threshold_1") == 0) config->temp_threshold_1 = (float)atof(value);
        else if (strcmp(name, "temp_threshold_2") == 0) config->temp_threshold_2 = (float)atof(value);
        else if (strcmp(name, "temp_threshold_3") == 0) config->temp_threshold_3 = (float)atof(value);
    }
    else if (strcmp(section, "temp_threshold_1_bar") == 0) {
        PARSE_COLOR(config->temp_threshold_1_bar, r);
        else PARSE_COLOR(config->temp_threshold_1_bar, g);
        else PARSE_COLOR(config->temp_threshold_1_bar, b);
    }
    else if (strcmp(section, "temp_threshold_2_bar") == 0) {
        PARSE_COLOR(config->temp_threshold_2_bar, r);
        else PARSE_COLOR(config->temp_threshold_2_bar, g);
        else PARSE_COLOR(config->temp_threshold_2_bar, b);
    }
    else if (strcmp(section, "temp_threshold_3_bar") == 0) {
        PARSE_COLOR(config->temp_threshold_3_bar, r);
        else PARSE_COLOR(config->temp_threshold_3_bar, g);
        else PARSE_COLOR(config->temp_threshold_3_bar, b);
    }
    else if (strcmp(section, "temp_threshold_4_bar") == 0) {
        PARSE_COLOR(config->temp_threshold_4_bar, r);
        else PARSE_COLOR(config->temp_threshold_4_bar, g);
        else PARSE_COLOR(config->temp_threshold_4_bar, b);
    }

    #undef SAFE_STRCPY
    #undef PARSE_COLOR
    
    return 1;
}

/**
 * @brief Loads configuration from INI file.
 * @details Parses the INI file and fills the Config struct. Returns 0 on success, -1 on error. Always check the return value.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/opt/coolerdash/config.ini") != 0) {
 *         // handle error
 *     }
 */
int load_config_ini(Config *config, const char *path)
{
    // Validate input parameters
    if (!config || !path) {
        return -1;
    }
    
    // Parse INI file and return success/failure
    return (ini_parse(path, inih_config_handler, config) < 0) ? -1 : 0;
}