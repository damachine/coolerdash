/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 *    This software is provided "as is", without warranty of any kind, express or implied.
 *    I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief Configuration management for CoolerDash LCD monitoring system.
 * @details Provides functions to load and apply configuration settings from an INI file with fallback support.
 */

// Include necessary headers
#ifndef CONFIG_H
#define CONFIG_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stdint.h>
#include <ini.h>
// cppcheck-suppress-end missingIncludeSystem

#define CONFIG_MAX_STRING_LEN 256
#define CONFIG_MAX_PASSWORD_LEN 128
#define CONFIG_MAX_PATH_LEN 512
#define CONFIG_MAX_FONT_NAME_LEN 64

/**
 * @brief Simple color structure.
 * @details Represents RGB color values with 8-bit components and padding for memory alignment.
 */
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t _pad;
} Color;

/**
 * @brief Common log levels for all modules.
 * @details Defines standardized logging levels used throughout the application for consistent message categorization.
 */
typedef enum
{
    LOG_INFO,
    LOG_STATUS,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

/**
 * @brief Configuration structure.
 * @details Contains all settings for the CoolerDash system, including paths, display settings, temperature thresholds, and visual styling options.
 */
typedef struct Config
{
    // Daemon configuration
    char daemon_address[CONFIG_MAX_STRING_LEN];
    char daemon_password[CONFIG_MAX_PASSWORD_LEN];

    // Paths configuration
    char paths_pid[CONFIG_MAX_PATH_LEN];
    char paths_images[CONFIG_MAX_PATH_LEN];
    char paths_image_coolerdash[CONFIG_MAX_PATH_LEN];
    char paths_image_shutdown[CONFIG_MAX_PATH_LEN];

    // Display configuration
    uint16_t display_width;
    uint16_t display_height;
    uint16_t display_refresh_interval_sec;
    uint32_t display_refresh_interval_nsec;
    uint8_t lcd_brightness;
    uint8_t lcd_orientation;

    // Layout configuration - all positioning is now calculated dynamically from display dimensions
    uint16_t layout_bar_width; // Legacy - not used in dynamic scaling
    uint16_t layout_bar_height;
    uint16_t layout_bar_gap;
    float layout_bar_border_width;
    Color layout_bar_color_background;
    Color layout_bar_color_border;

    // Font configuration
    char font_face[CONFIG_MAX_FONT_NAME_LEN];
    float font_size_temp;
    float font_size_labels;
    Color font_color_temp;
    Color font_color_label;

    // Temperature configuration
    float temp_threshold_1;
    float temp_threshold_2;
    float temp_threshold_3;
    Color temp_threshold_1_bar;
    Color temp_threshold_2_bar;
    Color temp_threshold_3_bar;
    Color temp_threshold_4_bar;
} Config;

/**
 * @brief Globale Logging-Funktion für alle Module außer main.c
 * @details Einheitliche Log-Ausgabe für Info, Status, Warnung und Fehler.
 */
void log_message(log_level_t level, const char *format, ...);

/**
 * @brief Global logging control from main.c
 * @details External variable controlling verbose logging behavior across all modules.
 */
extern int verbose_logging;

/**
 * @brief Initialize config structure with safe defaults.
 * @details Clears memory and applies safe fallback values for all configuration fields.
 */
void init_config_defaults(Config *config);

/**
 * @brief Loads configuration from INI file.
 * @details Parses INI configuration file and populates Config structure with fallback defaults.
 */
int load_config(const char *path, Config *config);

#endif // CONFIG_H