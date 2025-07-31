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

// Helper function for safe string copying
#define SAFE_STRCPY(dest, src) do { \
    strncpy(dest, src, sizeof(dest) - 1); \
    dest[sizeof(dest) - 1] = '\0'; \
} while(0)

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

    // Helper function for color parsing
    #define PARSE_COLOR(color_struct, component) \
        if (strcmp(name, #component) == 0) color_struct.component = atoi(value)

    // Daemon section
    if (strcmp(section, "daemon") == 0) {
        if (strcmp(name, "address") == 0) {
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->daemon_address, value);
            } else {
                SAFE_STRCPY(config->daemon_address, "http://localhost:11987");
            }
        } else if (strcmp(name, "password") == 0) {
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->daemon_password, value);
            } else {
                SAFE_STRCPY(config->daemon_password, "coolAdmin");
            }
        }
    }
    // Paths section
    else if (strcmp(section, "paths") == 0) {
        if (strcmp(name, "images") == 0) {
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->paths_images, value);
            } else {
                SAFE_STRCPY(config->paths_images, "/opt/coolerdash/images");
            }
        } else if (strcmp(name, "image_coolerdash") == 0) {
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->paths_image_coolerdash, value);
            } else {
                SAFE_STRCPY(config->paths_image_coolerdash, "/tmp/coolerdash.png");
            }
        } else if (strcmp(name, "image_shutdown") == 0) {
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->paths_image_shutdown, value);
            } else {
                SAFE_STRCPY(config->paths_image_shutdown, "/opt/coolerdash/images/shutdown.png");
            }
        } else if (strcmp(name, "pid") == 0) {
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->paths_pid, value);
            } else {
                SAFE_STRCPY(config->paths_pid, "/run/coolerdash/coolerdash.pid");
            }
        }
    }
    // Display section
    else if (strcmp(section, "display") == 0) {
        if (strcmp(name, "width") == 0) config->display_width = atoi(value);
        else if (strcmp(name, "height") == 0) config->display_height = atoi(value);
        else if (strcmp(name, "refresh_interval_sec") == 0) {
            config->display_refresh_interval_sec = (value && value[0] != '\0') ? atoi(value) : 2;
        }
        else if (strcmp(name, "refresh_interval_nsec") == 0) {
            config->display_refresh_interval_nsec = (value && value[0] != '\0') ? atoi(value) : 500000000;
        }
        else if (strcmp(name, "brightness") == 0) {
            config->lcd_brightness = (value && value[0] != '\0') ? atoi(value) : 80;
        }
        else if (strcmp(name, "orientation") == 0) {
            config->lcd_orientation = (value && value[0] != '\0') ? atoi(value) : 0;
        }
        else if (strcmp(name, "temp_1_update_threshold") == 0) {
            config->temp_1_update_threshold = (value && value[0] != '\0') ? (float)atof(value) : 1.0f;
        }
        else if (strcmp(name, "temp_2_update_threshold") == 0) {
            config->temp_2_update_threshold = (value && value[0] != '\0') ? (float)atof(value) : 1.0f;
        }
    }
    // Layout section
    else if (strcmp(section, "layout") == 0) {
        if (strcmp(name, "box_width") == 0) {
            config->layout_box_width = (value && value[0] != '\0') ? atoi(value) : config->display_width;
        }
        else if (strcmp(name, "box_height") == 0) {
            config->layout_box_height = (value && value[0] != '\0') ? atoi(value) : (config->display_height / 2);
        }
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
            if (value && value[0] != '\0') {
                SAFE_STRCPY(config->font_face, value);
            } else {
                SAFE_STRCPY(config->font_face, "Roboto Black");
            }
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

    #undef PARSE_COLOR
    
    return 1;
}

/*
 * @brief Sets fallback default values for missing or empty configuration fields.
 * @details This function should be called after parsing the INI file to ensure all important fields are set to sensible defaults if not provided.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/opt/coolerdash/config.ini") != 0) { // handle error }
 *     config_apply_fallbacks(&cfg);
 */
void config_apply_fallbacks(Config *config){
    if (!config) return;
    // Daemon
    if (config->daemon_address[0] == '\0') SAFE_STRCPY(config->daemon_address, "http://localhost:11987");
    if (config->daemon_password[0] == '\0') SAFE_STRCPY(config->daemon_password, "coolAdmin");
    // Paths
    if (config->paths_images[0] == '\0') SAFE_STRCPY(config->paths_images, "/opt/coolerdash/images");
    if (config->paths_image_coolerdash[0] == '\0') SAFE_STRCPY(config->paths_image_coolerdash, "/tmp/coolerdash.png");
    if (config->paths_image_shutdown[0] == '\0') SAFE_STRCPY(config->paths_image_shutdown, "/opt/coolerdash/images/shutdown.png");
    if (config->paths_pid[0] == '\0') SAFE_STRCPY(config->paths_pid, "/run/coolerdash/coolerdash.pid");
    // Display
    if (config->display_width == 0) config->display_width = 240;
    if (config->display_height == 0) config->display_height = 240;
    if (config->display_refresh_interval_sec == 0) config->display_refresh_interval_sec = 2;
    if (config->display_refresh_interval_nsec == 0) config->display_refresh_interval_nsec = 500000000;
    if (config->lcd_brightness == 0) config->lcd_brightness = 80;
    if (config->temp_1_update_threshold == 0.0f) config->temp_1_update_threshold = 1.0f;
    if (config->temp_2_update_threshold == 0.0f) config->temp_2_update_threshold = 1.0f;
    // Layout
    if (config->layout_box_width == 0) config->layout_box_width = config->display_width;
    if (config->layout_box_height == 0) config->layout_box_height = config->display_height / 2;
    if (config->layout_bar_width == 0) config->layout_bar_width = config->layout_box_width - 10;
    if (config->layout_bar_height == 0) config->layout_bar_height = 22;
    if (config->layout_bar_gap == 0) config->layout_bar_gap = 10;
    if (config->layout_bar_border_width == 0.0f) config->layout_bar_border_width = 1.5f;
    // Font
    if (config->font_face[0] == '\0') SAFE_STRCPY(config->font_face, "Roboto Black");
    if (config->font_size_temp == 0.0f) config->font_size_temp = 100.0f;
    if (config->font_size_labels == 0.0f) config->font_size_labels = 30.0f;
    // Temperature thresholds
    if (config->temp_threshold_1 == 0.0f) config->temp_threshold_1 = 55.0f;
    if (config->temp_threshold_2 == 0.0f) config->temp_threshold_2 = 65.0f;
    if (config->temp_threshold_3 == 0.0f) config->temp_threshold_3 = 75.0f;
    // Colors
    if (config->layout_bar_color_background.r == 0 && config->layout_bar_color_background.g == 0 && config->layout_bar_color_background.b == 0) {
        config->layout_bar_color_background.r = 64;
        config->layout_bar_color_background.g = 64;
        config->layout_bar_color_background.b = 64;
    }
    if (config->layout_bar_color_border.r == 0 && config->layout_bar_color_border.g == 0 && config->layout_bar_color_border.b == 0) {
        config->layout_bar_color_border.r = 192;
        config->layout_bar_color_border.g = 192;
        config->layout_bar_color_border.b = 192;
    }
    if (config->font_color_temp.r == 0 && config->font_color_temp.g == 0 && config->font_color_temp.b == 0) {
        config->font_color_temp.r = 255;
        config->font_color_temp.g = 255;
        config->font_color_temp.b = 255;
    }
    if (config->font_color_label.r == 0 && config->font_color_label.g == 0 && config->font_color_label.b == 0) {
        config->font_color_label.r = 200;
        config->font_color_label.g = 200;
        config->font_color_label.b = 200;
    }
    // Temperature bar colors
    if (config->temp_threshold_1_bar.r == 0 && config->temp_threshold_1_bar.g == 0 && config->temp_threshold_1_bar.b == 0) {
        config->temp_threshold_1_bar.r = 0;
        config->temp_threshold_1_bar.g = 255;
        config->temp_threshold_1_bar.b = 0;
    }
    if (config->temp_threshold_2_bar.r == 0 && config->temp_threshold_2_bar.g == 0 && config->temp_threshold_2_bar.b == 0) {
        config->temp_threshold_2_bar.r = 255;
        config->temp_threshold_2_bar.g = 140;
        config->temp_threshold_2_bar.b = 0;
    }
    if (config->temp_threshold_3_bar.r == 0 && config->temp_threshold_3_bar.g == 0 && config->temp_threshold_3_bar.b == 0) {
        config->temp_threshold_3_bar.r = 255;
        config->temp_threshold_3_bar.g = 70;
        config->temp_threshold_3_bar.b = 0;
    }
    if (config->temp_threshold_4_bar.r == 0 && config->temp_threshold_4_bar.g == 0 && config->temp_threshold_4_bar.b == 0) {
        config->temp_threshold_4_bar.r = 255;
        config->temp_threshold_4_bar.g = 0;
        config->temp_threshold_4_bar.b = 0;
    }
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
    int result = (ini_parse(path, inih_config_handler, config) < 0) ? -1 : 0;
    config_apply_fallbacks(config);
    return result;
}

