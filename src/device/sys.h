/**
* -----------------------------------------------------------------------------
* Created by: damachine (christkue79 at gmail dot com)
* Website: https://github.com/damachine/coolerdash
* -----------------------------------------------------------------------------
 */

/**
 * @brief System default configuration values and core types.
 * @details Provides hardcoded fallback values for all CoolerDash configuration parameters.
 *          These values are always available and serve as baseline when user config is missing or incomplete.
 */

#ifndef SYS_H
#define SYS_H

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
    float display_refresh_interval; // Refresh interval in seconds (e.g., 2.50 = 2.5 seconds)
    uint8_t lcd_brightness;
    uint8_t lcd_orientation;
    // Developer/testing override: force display to be treated as circular (1) or not (0)
    int force_display_circular;
    // Display shape override: "auto" (default), "rectangular", or "circular"
    char display_shape[16];
    // Display mode: "dual" (default) or "circle" (alternating single sensor)
    char display_mode[16];
    // Circle mode sensor switch interval (seconds) - default: 5
    uint16_t circle_switch_interval;
    // Content scale factor (0.0-1.0) - how much of safe area to use - default: 0.98
    float display_content_scale_factor;
    // Custom inscribe factor for circular displays (>=0 && <=1.0)
    // - 0.0 = auto (use geometric inscribe factor 1/√2 ≈ 0.7071)
    // Default: 0.70710678 (geometric inscribe 1/√2)
    float display_inscribe_factor;

    // Layout configuration - all positioning is calculated dynamically from display dimensions
    uint16_t layout_bar_height;
    uint16_t layout_bar_gap;
    float layout_bar_border;
    uint8_t layout_bar_width;         // Bar width as percentage of display width (1-100), default: 98
    uint8_t layout_label_margin_left; // Label distance from left screen edge (1-50%, default: 1)
    uint8_t layout_label_margin_bar;  // Label distance from bars (1-20%, default: 1)
    Color layout_bar_color_background;
    Color layout_bar_color_border;

    // Font configuration
    char font_face[CONFIG_MAX_FONT_NAME_LEN];
    float font_size_temp;
    float font_size_labels;
    Color font_color_temp;
    Color font_color_label;

    // Display positioning overrides (optional - set to -9999 for auto)
    int display_temp_offset_x_cpu;    // Horizontal offset for CPU temperature
    int display_temp_offset_x_gpu;    // Horizontal offset for GPU temperature
    int display_temp_offset_y_cpu;    // Vertical offset for CPU temperature
    int display_temp_offset_y_gpu;    // Vertical offset for GPU temperature
    int display_temp_offset_x_liquid; // Horizontal offset for Liquid temperature
    int display_temp_offset_y_liquid; // Vertical offset for Liquid temperature
    int display_degree_spacing;       // Spacing between temperature and degree symbol (pixels, default: 16)
    int display_label_offset_x;       // Horizontal offset for CPU/GPU labels
    int display_label_offset_y;       // Vertical offset for CPU/GPU labels

    // Temperature configuration
    float temp_threshold_1;
    float temp_threshold_2;
    float temp_threshold_3;
    float temp_max_scale; // Maximum temperature for bar scaling (default: 115°C)
    Color temp_threshold_1_bar;
    Color temp_threshold_2_bar;
    Color temp_threshold_3_bar;
    Color temp_threshold_4_bar;

    // Liquid temperature configuration (for circle mode, shown with CPU)
    float temp_liquid_max_scale;       // Maximum liquid temperature for bar scaling (default: 50°C)
    float temp_liquid_threshold_1;     // Liquid temp threshold 1 (default: 25°C)
    float temp_liquid_threshold_2;     // Liquid temp threshold 2 (default: 30°C)
    float temp_liquid_threshold_3;     // Liquid temp threshold 3 (default: 35°C)
    Color temp_liquid_threshold_1_bar; // Bar color for liquid < threshold_1
    Color temp_liquid_threshold_2_bar; // Bar color for threshold_1 <= liquid < threshold_2
    Color temp_liquid_threshold_3_bar; // Bar color for threshold_2 <= liquid < threshold_3
    Color temp_liquid_threshold_4_bar; // Bar color for liquid >= threshold_3
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
// Common Helper Macros and Inline Functions
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
    return (orientation == 0 || orientation == 90 || orientation == 180 || orientation == 270);
}

// ============================================================================
// System Configuration Functions
// ============================================================================

/**
 * @brief Initialize config structure with system defaults.
 * @details Clears config memory and applies hardcoded system default values.
 *          This function always succeeds and provides a valid baseline configuration.
 */
void init_system_defaults(Config *config);

/**
 * @brief Apply system default values for missing/unset fields.
 * @details Sets fallback values only for fields that are zero/empty (not configured by user).
 *          This allows user configuration to take precedence while providing safe defaults.
 *
 *          Call order: 1) init_system_defaults() 2) load user config 3) apply_system_defaults()
 */
void apply_system_defaults(Config *config);

#endif // SYS_H
