/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CoolerControl API interface for LCD device communication.
 * @details Provides functions for initializing, authenticating, and communicating with CoolerControl LCD devices.
 * @example
 *     See function documentation for usage examples.
 */

// Function prototypes
#ifndef CONFIG_H
#define CONFIG_H

// Include necessary headers
#include <stdint.h>
#include <ini.h>

/**
 * @brief Color struct for RGB values (0-255).
 * @details Used for all color configuration values in CoolerDash.
 * @example
 *     Color green = {0, 255, 0};
 *     Color red = {255, 0, 0};
 *     // Use in config: cfg.color_green = green;
 */
typedef struct {
    int r; // Red value (0-255)
    int g; // Green value (0-255)
    int b; // Blue value (0-255)
} Color;

/**
 * @brief Structure for runtime configuration loaded from INI file.
 * @details All fields are loaded from the INI file.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/opt/coolerdash/config.ini") == 0) {
 *         // Use cfg fields, e.g. cfg.display_width
 *     }
 */
typedef struct Config {
    // General configuration
    char daemon_address[128];
    char daemon_password[64];
    ////////////////////////////////////////////////////////////////////////////////
    // Paths configuration
    char paths_pid[128];
    char paths_hwmon[128];
    char paths_images[128];
    char paths_image_coolerdash[128];
    char paths_image_shutdown[128];
    ////////////////////////////////////////////////////////////////////////////////
    // Display configuration
    int display_width;
    int display_height;
    int display_refresh_interval_sec;
    int display_refresh_interval_nsec;
    int lcd_brightness;
    int lcd_orientation;
    ////////////////////////////////////////////////////////////////////////////////
    // Layout configuration
    int layout_box_width;
    int layout_box_height;
    int layout_box_gap;
    int layout_bar_width;
    int layout_bar_height;
    int layout_bar_gap;
    float layout_bar_border_width;
    Color layout_bar_color_background;
    Color layout_bar_color_border;
    ////////////////////////////////////////////////////////////////////////////////
    // Font configuration
    char font_face[64];
    float font_size_temp;
    float font_size_labels;
    Color font_color_temp;
    Color font_color_label;
    ////////////////////////////////////////////////////////////////////////////////
    // Temperature configuration
    float temp_threshold_1;
    float temp_threshold_2;
    float temp_threshold_3;
    Color temp_threshold_1_bar;
    Color temp_threshold_2_bar;
    Color temp_threshold_3_bar;
    Color temp_threshold_4_bar;
    ////////////////////////////////////////////////////////////////////////////////
} Config;

/**
 * @brief Loads configuration from INI file.
 * @details Loads all configuration values from the specified INI file and populates the given Config structure. Returns 0 on success, -1 on error. Always check the return value.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/opt/coolerdash/config.ini") != 0) {
 *         // handle error
 *     }
 */
int load_config_ini(Config *config, const char *path);

#endif // CONFIG_H