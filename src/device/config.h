/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief CoolerControl Plugin Configuration System
 * @details Loads all configuration from config.json with hardcoded defaults as fallback.
 *          Replaces the old config.ini + sys.c system with a unified JSON-based approach.
 */

#ifndef PLUGIN_H
#define PLUGIN_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stdint.h>
// cppcheck-suppress-end missingIncludeSystem

// Configuration constants
#define CONFIG_MAX_STRING_LEN 256
#define CONFIG_MAX_PASSWORD_LEN 128
#define CONFIG_MAX_PATH_LEN 512
#define CONFIG_MAX_FONT_NAME_LEN 64

/**
 * @brief Simple color structure.
 * @details Represents RGB color values with 8-bit components and padding for
 * memory alignment.
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
 * @details Defines standardized logging levels used throughout the application
 * for consistent message categorization.
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
 * @details Contains all settings for the CoolerDash system, including paths,
 * display settings, temperature thresholds, and visual styling options.
 */
typedef struct Config
{
    // Daemon configuration
    char daemon_address[CONFIG_MAX_STRING_LEN];
    char daemon_password[CONFIG_MAX_PASSWORD_LEN];

    // Paths configuration
    char paths_images[CONFIG_MAX_PATH_LEN];
    char paths_image_coolerdash[CONFIG_MAX_PATH_LEN];
    char paths_image_shutdown[CONFIG_MAX_PATH_LEN];

    // Display configuration
    uint16_t display_width;
    uint16_t display_height;
    float display_refresh_interval;
    uint8_t lcd_brightness;
    uint8_t lcd_orientation;
    int force_display_circular;
    char display_shape[16];
    char display_mode[16];
    uint16_t circle_switch_interval;
    float display_content_scale_factor;
    float display_inscribe_factor;

    // Layout configuration
    uint16_t layout_bar_height;
    uint16_t layout_bar_gap;
    float layout_bar_border;
    uint8_t layout_bar_width;
    uint8_t layout_label_margin_left;
    uint8_t layout_label_margin_bar;
    Color layout_bar_color_background;
    Color layout_bar_color_border;

    // Font configuration
    char font_face[CONFIG_MAX_FONT_NAME_LEN];
    float font_size_temp;
    float font_size_labels;
    Color font_color_temp;
    Color font_color_label;

    // Display positioning overrides
    int display_temp_offset_x_cpu;
    int display_temp_offset_x_gpu;
    int display_temp_offset_y_cpu;
    int display_temp_offset_y_gpu;
    int display_temp_offset_x_liquid;
    int display_temp_offset_y_liquid;
    int display_degree_spacing;
    int display_label_offset_x;
    int display_label_offset_y;

    // Temperature configuration
    float temp_threshold_1;
    float temp_threshold_2;
    float temp_threshold_3;
    float temp_max_scale;
    Color temp_threshold_1_bar;
    Color temp_threshold_2_bar;
    Color temp_threshold_3_bar;
    Color temp_threshold_4_bar;

    // Liquid temperature configuration
    float temp_liquid_max_scale;
    float temp_liquid_threshold_1;
    float temp_liquid_threshold_2;
    float temp_liquid_threshold_3;
    Color temp_liquid_threshold_1_bar;
    Color temp_liquid_threshold_2_bar;
    Color temp_liquid_threshold_3_bar;
    Color temp_liquid_threshold_4_bar;
} Config;

/**
 * @brief Global logging function for all modules except main.c
 * @details Provides unified log output for info, status, warning and error messages.
 */
void log_message(log_level_t level, const char *format, ...);

/**
 * @brief Global logging control from main.c
 * @details External variable controlling verbose logging behavior across all modules.
 */
extern int verbose_logging;

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Safe string copy macro using project's cc_safe_strcpy.
 * @details Automatically uses sizeof(dest) for bounds checking.
 */
#define SAFE_STRCPY(dest, src)                       \
    do                                               \
    {                                                \
        cc_safe_strcpy((dest), sizeof(dest), (src)); \
    } while (0)

/**
 * @brief Validate LCD orientation value.
 * @details Checks if orientation is one of: 0°, 90°, 180°, 270°.
 */
static inline int is_valid_orientation(int orientation)
{
    return (orientation == 0 || orientation == 90 || orientation == 180 ||
            orientation == 270);
}

// ============================================================================
// Plugin Configuration Functions
// ============================================================================

/**
 * @brief Load complete configuration from config.json with hardcoded defaults
 * @param config Pointer to Config struct to populate
 * @param config_path Optional path to config.json (NULL = use default location)
 * @return 1 on success (config loaded), 0 if using defaults only
 *
 * This function:
 * 1. Initializes config with hardcoded defaults
 * 2. Tries to load config.json from plugin directory
 * 3. Applies all settings with validation
 * 4. Returns ready-to-use config (always succeeds)
 *
 * Default config.json location:
 *   /etc/coolercontrol/plugins/coolerdash/config.json
 */
int load_plugin_config(Config *config, const char *config_path);

#endif // PLUGIN_H
