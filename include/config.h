/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Configuration management for CoolerDash LCD monitoring system.
 * @details Provides functions to load, validate, and apply configuration settings from an INI file with comprehensive validation and fallback support.
 */

// Include necessary headers
#ifndef CONFIG_H
#define CONFIG_H

// Basic constants
#define CONFIG_MAX_BRIGHTNESS 100
#define CONFIG_MAX_DISPLAY_SIZE 4096
#define CONFIG_MAX_FONT_NAME_LEN 64
#define CONFIG_MAX_FONT_SIZE 128
#define CONFIG_MAX_PASSWORD_LEN 128
#define CONFIG_MAX_PATH_LEN 512
#define CONFIG_MAX_STRING_LEN 256
#define CONFIG_MAX_TEMP 150.0f
#define CONFIG_MIN_DISPLAY_SIZE 32
#define CONFIG_MIN_FONT_SIZE 6
#define CONFIG_MIN_TEMP -50.0f

#include <ini.h>
#include <stdint.h>

/**
 * @brief Simple color structure.
 * @details Represents RGB color values with 8-bit components and padding for memory alignment.
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t _pad;
} Color;

/**
 * @brief Configuration structure.
 * @details Contains all settings for the CoolerDash system, including paths, display settings, temperature thresholds, and visual styling options.
 */
typedef struct Config {
    // General configuration
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
    
    // Layout configuration
    uint16_t layout_box_width;
    uint16_t layout_box_height;
    uint16_t layout_box_gap;
    uint16_t layout_bar_width;
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
    
////////////////////////////////////////////////////////////////////////////////
} Config;

/**
 * @brief Loads configuration from INI file.
 * @details Parses INI configuration file and populates Config structure with validated values and fallback defaults.
 */
int load_config(const char *path, Config *config);

/**
 * @brief Legacy function name for backward compatibility.
 * @details Provides backward compatibility by calling load_config with swapped parameter order.
 */
static inline int load_config_ini(Config *config, const char *path) {
    return load_config(path, config);
}

/**
 * @brief Validate complete configuration structure.
 * @details Performs comprehensive validation of all configuration fields including ranges, paths, and dependencies.
 */
int config_validate(const Config *config);

/**
 * @brief Validate color structure.
 * @details Checks if color pointer is valid (basic null pointer validation).
 */
static inline int config_validate_color(const Color *color) {
    return (color != NULL);
}

/**
 * @brief Initialize config structure with safe defaults.
 * @details Clears memory and applies safe fallback values for all configuration fields.
 */
void config_init_defaults(Config *config);

/**
 * @brief Validate display dimensions.
 * @details Checks if width and height are within acceptable ranges for LCD display rendering.
 */
static inline int config_validate_display_size(uint16_t width, uint16_t height) {
    return (width >= CONFIG_MIN_DISPLAY_SIZE && width <= CONFIG_MAX_DISPLAY_SIZE &&
            height >= CONFIG_MIN_DISPLAY_SIZE && height <= CONFIG_MAX_DISPLAY_SIZE);
}

/**
 * @brief Validate temperature value.
 * @details Checks if temperature is within reasonable sensor reading range.
 */
static inline int config_validate_temperature(float temperature) {
    return (temperature >= CONFIG_MIN_TEMP && temperature <= CONFIG_MAX_TEMP);
}

/**
 * @brief Clamp value to valid range.
 * @details Constrains integer value to specified minimum and maximum bounds.
 */
static inline int config_clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Global logging control from main.c
 * @details External variable controlling verbose logging behavior across all modules.
 */
extern int verbose_logging;

/**
 * @brief Common log levels for all modules.
 * @details Defines standardized logging levels used throughout the application for consistent message categorization.
 */
typedef enum {
    LOG_INFO,
    LOG_STATUS,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

/**
 * @brief Clamp float value to valid range.
 * @details Constrains floating-point value to specified minimum and maximum bounds.
 */
static inline float config_clamp_float(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

#endif // CONFIG_H