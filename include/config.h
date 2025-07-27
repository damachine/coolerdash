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
 *     if (load_config_ini(&cfg, "/etc/coolerdash/config.ini") == 0) {
 *         // Use cfg fields, e.g. cfg.display_width
 *     }
 */
typedef struct Config {
    // General configuration
    char daemon_address[128];    // Daemon address
    char daemon_password[64];    // Daemon password
    ////////////////////////////////////////////////////////////////////////////////
    // Paths configuration
    char paths_pid[128];               // Path for PID file
    char paths_hwmon[128];        // Path to hwmon
    char paths_images[128];            // Directory for images
    char paths_image_coolerdash[128];  // Path for display image
    char paths_image_shutdown[128];    // Path for shutdown image
    ////////////////////////////////////////////////////////////////////////////////
    // Display configuration
    int display_width;           // Display width in pixels
    int display_height;          // Display height in pixels
    int display_refresh_interval_sec; // Refresh interval (seconds)
    int display_refresh_interval_nsec; // Refresh interval (nanoseconds)
    int lcd_brightness;          // LCD brightness (0-100)
    int lcd_orientation;         // LCD orientation: 0, 90, 180, 270
    ////////////////////////////////////////////////////////////////////////////////
    // Layout configuration
    int box_width;               // Box width in pixels
    int box_height;              // Box height in pixels
    int box_gap;                 // Gap between boxes in pixels
    int bar_width;               // Bar width in pixels
    int bar_height;              // Bar height in pixels
    int bar_gap;                 // Gap between bars in pixels
    float bar_border_width;      // Border line width in pixels
    Color bar_color_background;  // RGB for bar background
    Color bar_color_border;      // RGB for bar border
    ////////////////////////////////////////////////////////////////////////////////
    // Font configuration
    char font_face[64];          // Font face for display text
    float font_size_temp;        // Temperature font size
    float font_size_labels;      // Label font size
    Color color_txt_temp;     // RGB for temperature text
    Color color_txt_label;    // RGB for label text
    ////////////////////////////////////////////////////////////////////////////////
    // Temperature configuration
    float temp_threshold_1;  // Green threshold (°C)
    float temp_threshold_2; // Orange threshold (°C)
    float temp_threshold_3; // Red threshold (°C)
    Color temp_threshold_1_bar;    // RGB for green bar
    Color temp_threshold_2_bar;    // RGB for orange bar
    Color temp_threshold_3_bar;    // RGB for hot orange bar
    Color temp_threshold_4_bar;    // RGB for red bar
    ////////////////////////////////////////////////////////////////////////////////
} Config;

/**
 * @brief Loads configuration from INI file.
 * @details Loads all configuration values from the specified INI file and populates the given Config structure. Returns 0 on success, -1 on error. Always check the return value.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/etc/coolerdash/config.ini") != 0) {
 *         // handle error
 *     }
 */
int load_config_ini(Config *config, const char *path);

#endif // CONFIG_H