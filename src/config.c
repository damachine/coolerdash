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
#include <string.h>
#include <stdlib.h>

/**
 * @brief INI parser handler, sets values in Config struct.
 * @details Called for each key-value pair in the INI file. Matches section and key names and sets the corresponding field in the Config struct. Unrecognized keys are ignored. For string fields, strncpy is used and buffers are always null-terminated for safety.
 * @example
 *     ini_parse("/etc/coolerdash/config.ini", inih_config_handler, &cfg);
 */
static int inih_config_handler(void *user, const char *section, const char *name, const char *value)
{
    Config *config = (Config *)user;

    // General section
    if (strcmp(section, "daemon") == 0) {
        if (strcmp(name, "address") == 0) {
            strncpy(config->daemon_address, value, sizeof(config->daemon_address) - 1);
            config->daemon_address[sizeof(config->daemon_address) - 1] = '\0';
        }
        if (strcmp(name, "password") == 0) {
            strncpy(config->daemon_password, value, sizeof(config->daemon_password) - 1);
            config->daemon_password[sizeof(config->daemon_password) - 1] = '\0';
        }
    }

    // Paths section
    if (strcmp(section, "paths") == 0) {
        if (strcmp(name, "hwmon") == 0) {
            strncpy(config->paths_hwmon, value, sizeof(config->paths_hwmon) - 1);
            config->paths_hwmon[sizeof(config->paths_hwmon) - 1] = '\0';
        }
        if (strcmp(name, "images") == 0) {
            strncpy(config->paths_images, value, sizeof(config->paths_images) - 1);
            config->paths_images[sizeof(config->paths_images) - 1] = '\0';
        }
        if (strcmp(name, "image_coolerdash") == 0) {
            strncpy(config->paths_image_coolerdash, value, sizeof(config->paths_image_coolerdash) - 1);
            config->paths_image_coolerdash[sizeof(config->paths_image_coolerdash) - 1] = '\0';
        }
        if (strcmp(name, "image_shutdown") == 0) {
            strncpy(config->paths_image_shutdown, value, sizeof(config->paths_image_shutdown) - 1);
            config->paths_image_shutdown[sizeof(config->paths_image_shutdown) - 1] = '\0';
        }
        if (strcmp(name, "pid") == 0) {
            strncpy(config->paths_pid, value, sizeof(config->paths_pid) - 1);
            config->paths_pid[sizeof(config->paths_pid) - 1] = '\0';
        }
    }

    // Display section
    if (strcmp(section, "display") == 0) {
        if (strcmp(name, "width") == 0) config->display_width = atoi(value);
        if (strcmp(name, "height") == 0) config->display_height = atoi(value);
        if (strcmp(name, "refresh_interval_sec") == 0) config->display_refresh_interval_sec = atoi(value);
        if (strcmp(name, "refresh_interval_nsec") == 0) config->display_refresh_interval_nsec = atoi(value);
        if (strcmp(name, "brightness") == 0) config->lcd_brightness = atoi(value);
        if (strcmp(name, "orientation") == 0) config->lcd_orientation = atoi(value);
    }

    // Layout section
    if (strcmp(section, "layout") == 0) {
        if (strcmp(name, "box_width") == 0) config->box_width = atoi(value);
        if (strcmp(name, "box_height") == 0) config->box_height = atoi(value);
        if (strcmp(name, "box_gap") == 0) config->box_gap = atoi(value);
        if (strcmp(name, "bar_width") == 0) config->bar_width = atoi(value);
        if (strcmp(name, "bar_height") == 0) config->bar_height = atoi(value);
        if (strcmp(name, "bar_gap") == 0) config->bar_gap = atoi(value);
        if (strcmp(name, "bar_border_width") == 0) config->bar_border_width = (float)atof(value);
    }
    if (strcmp(section, "bar_color_background") == 0) {
        if (strcmp(name, "r") == 0) config->bar_color_background.r = atoi(value);
        if (strcmp(name, "g") == 0) config->bar_color_background.g = atoi(value);
        if (strcmp(name, "b") == 0) config->bar_color_background.b = atoi(value);
    }
    if (strcmp(section, "bar_color_border") == 0) {
        if (strcmp(name, "r") == 0) config->bar_color_border.r = atoi(value);
        if (strcmp(name, "g") == 0) config->bar_color_border.g = atoi(value);
        if (strcmp(name, "b") == 0) config->bar_color_border.b = atoi(value);
    }

    // Font section
    if (strcmp(section, "font") == 0) {
        if (strcmp(name, "face") == 0) {
            strncpy(config->font_face, value, sizeof(config->font_face) - 1);
            config->font_face[sizeof(config->font_face) - 1] = '\0';
        }
        if (strcmp(name, "size_temp") == 0) config->font_size_temp = (float)atof(value);
        if (strcmp(name, "size_labels") == 0) config->font_size_labels = (float)atof(value);
    }
    if (strcmp(section, "color_txt_temp") == 0) {
        if (strcmp(name, "r") == 0) config->color_txt_temp.r = atoi(value);
        if (strcmp(name, "g") == 0) config->color_txt_temp.g = atoi(value);
        if (strcmp(name, "b") == 0) config->color_txt_temp.b = atoi(value);
    }
    if (strcmp(section, "color_txt_label") == 0) {
        if (strcmp(name, "r") == 0) config->color_txt_label.r = atoi(value);
        if (strcmp(name, "g") == 0) config->color_txt_label.g = atoi(value);
        if (strcmp(name, "b") == 0) config->color_txt_label.b = atoi(value);
    }

    // Temperature section
    if (strcmp(section, "temperature") == 0) {
        if (strcmp(name, "temp_threshold_1") == 0) config->temp_threshold_1 = (float)atof(value);
        if (strcmp(name, "temp_threshold_2") == 0) config->temp_threshold_2 = (float)atof(value);
        if (strcmp(name, "temp_threshold_3") == 0) config->temp_threshold_3 = (float)atof(value);
    }
    if (strcmp(section, "temp_threshold_1_bar") == 0) {
        if (strcmp(name, "r") == 0) config->temp_threshold_1_bar.r = atoi(value);
        if (strcmp(name, "g") == 0) config->temp_threshold_1_bar.g = atoi(value);
        if (strcmp(name, "b") == 0) config->temp_threshold_1_bar.b = atoi(value);
    }
    if (strcmp(section, "temp_threshold_2_bar") == 0) {
        if (strcmp(name, "r") == 0) config->temp_threshold_2_bar.r = atoi(value);
        if (strcmp(name, "g") == 0) config->temp_threshold_2_bar.g = atoi(value);
        if (strcmp(name, "b") == 0) config->temp_threshold_2_bar.b = atoi(value);
    }
    if (strcmp(section, "temp_threshold_3_bar") == 0) {
        if (strcmp(name, "r") == 0) config->temp_threshold_3_bar.r = atoi(value);
        if (strcmp(name, "g") == 0) config->temp_threshold_3_bar.g = atoi(value);
        if (strcmp(name, "b") == 0) config->temp_threshold_3_bar.b = atoi(value);
    }
    if (strcmp(section, "temp_threshold_4_bar") == 0) {
        if (strcmp(name, "r") == 0) config->temp_threshold_4_bar.r = atoi(value);
        if (strcmp(name, "g") == 0) config->temp_threshold_4_bar.g = atoi(value);
        if (strcmp(name, "b") == 0) config->temp_threshold_4_bar.b = atoi(value);
    }
    return 1;
}

/**
 * @brief Loads configuration from INI file.
 * @details Parses the INI file and fills the Config struct. Returns 0 on success, -1 on error. Always check the return value.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/etc/coolerdash/config.ini") != 0) {
 *         // handle error
 *     }
 */
int load_config_ini(Config *config, const char *path)
{
    if (!config || !path) return -1;
    int error = ini_parse(path, inih_config_handler, config);
    if (error < 0) {
        return -1;
    }
    return 0;
}