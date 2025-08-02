/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Configuration management for CoolerDash LCD monitoring system.
 * @details Provides secure configuration loading with validation and optimized data structures.
 * Enhanced with buffer overflow protection, input validation, and cache-friendly alignment.
 * @example
 *     Config cfg;
 *     if (load_config("/opt/coolerdash/config.ini", &cfg) == 0) {
 *         // Use cfg fields safely with built-in validation
 *     }
 */

// Include necessary headers
#ifndef CONFIG_H
#define CONFIG_H

#include <ini.h>
#include <stdint.h>

// Security and validation constants
#define CONFIG_MAX_STRING_LEN 256          // Maximum length for general strings
#define CONFIG_MAX_PASSWORD_LEN 128        // Maximum password length for security
#define CONFIG_MAX_PATH_LEN 512            // Maximum path length (Linux PATH_MAX consideration)
#define CONFIG_MAX_FONT_NAME_LEN 64        // Maximum font name length
#define CONFIG_MIN_DISPLAY_SIZE 32         // Minimum display dimension
#define CONFIG_MAX_DISPLAY_SIZE 4096       // Maximum display dimension
#define CONFIG_MIN_TEMP -50.0f             // Minimum valid temperature (°C)
#define CONFIG_MAX_TEMP 150.0f             // Maximum valid temperature (°C)
#define CONFIG_MAX_BRIGHTNESS 100          // Maximum LCD brightness value
#define CONFIG_MAX_FONT_SIZE 128           // Maximum font size
#define CONFIG_MIN_FONT_SIZE 6             // Minimum readable font size

/**
 * @brief Optimized color structure with memory efficiency and validation.
 * @details Uses uint8_t for automatic range validation and better memory layout.
 * Packed structure for minimal memory footprint with cache alignment.
 * @example
 *     Color green = {0, 255, 0};
 *     Color red = {255, 0, 0};
 */
typedef struct __attribute__((packed)) {
    uint8_t r;      // Red value (0-255, automatically validated by type)
    uint8_t g;      // Green value (0-255, automatically validated by type)  
    uint8_t b;      // Blue value (0-255, automatically validated by type)
    uint8_t _pad;   // Padding for 4-byte alignment and future alpha channel
} Color;

/**
 * @brief Optimized configuration structure with security enhancements.
 * @details Cache-aligned structure with proper data types and buffer overflow protection.
 * Uses fixed-size buffers and appropriate integer types for better performance and security.
 * @example
 *     Config cfg;
 *     if (load_config("/opt/coolerdash/config.ini", &cfg) == 0) {
 *         // Use cfg fields safely
 *     }
 */
typedef struct __attribute__((aligned(8))) Config {  // 8-byte alignment for cache efficiency
    // General configuration - using fixed-size arrays for security
    char daemon_address[CONFIG_MAX_STRING_LEN];
    char daemon_password[CONFIG_MAX_PASSWORD_LEN];
    
    ////////////////////////////////////////////////////////////////////////////////
    // Paths configuration - using maximum path length for security
    char paths_pid[CONFIG_MAX_PATH_LEN];
    char paths_images[CONFIG_MAX_PATH_LEN];  
    char paths_image_coolerdash[CONFIG_MAX_PATH_LEN];
    char paths_image_shutdown[CONFIG_MAX_PATH_LEN];
    
    ////////////////////////////////////////////////////////////////////////////////
    // Display configuration - using appropriate integer types
    uint16_t display_width;                       // Using uint16_t for display dimensions
    uint16_t display_height;
    uint16_t display_refresh_interval_sec;        // Using uint16_t for intervals
    uint32_t display_refresh_interval_nsec;       // Using uint32_t for nanoseconds
    uint8_t lcd_brightness;                       // Using uint8_t for brightness (0-100)
    uint8_t lcd_orientation;                      // Using uint8_t for orientation
    float temp_1_update_threshold;
    float temp_2_update_threshold;
    
    ////////////////////////////////////////////////////////////////////////////////
    // Layout configuration - using uint16_t for layout dimensions
    uint16_t layout_box_width;
    uint16_t layout_box_height;
    uint16_t layout_box_gap;
    uint16_t layout_bar_width;
    uint16_t layout_bar_height;
    uint16_t layout_bar_gap;
    float layout_bar_border_width;
    Color layout_bar_color_background;
    Color layout_bar_color_border;
    
    ////////////////////////////////////////////////////////////////////////////////
    // Font configuration - using fixed-size buffer for security
    char font_face[CONFIG_MAX_FONT_NAME_LEN];
    float font_size_temp;
    float font_size_labels;
    Color font_color_temp;
    Color font_color_label;
    
    ////////////////////////////////////////////////////////////////////////////////
    // Temperature configuration - using float for temperature values
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
 * @brief Loads configuration from INI file with comprehensive validation.
 * @details Enhanced with input validation, buffer overflow protection, and secure defaults.
 * Modern interface with parameter order optimized for safety (path first, config second).
 * @example
 *     Config cfg;
 *     if (load_config("/opt/coolerdash/config.ini", &cfg) != 0) {
 *         // handle error
 *     }
 */
int load_config(const char *path, Config *config);

/**
 * @brief Legacy function name for backward compatibility.
 * @details Wrapper around load_config() for existing code compatibility.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/opt/coolerdash/config.ini") != 0) { ... }
 */
static inline int load_config_ini(Config *config, const char *path) {
    return load_config(path, config);
}

/**
 * @brief Validate complete configuration structure.
 * @details Performs comprehensive validation of all configuration fields.
 * @example
 *     if (config_validate(&cfg)) { ... }
 */
int config_validate(const Config *config);

/**
 * @brief Validate color structure.
 * @details Checks if RGB values are within valid range (automatically handled by uint8_t).
 * @example
 *     if (config_validate_color(&color)) { ... }
 */
static inline int config_validate_color(const Color *color) {
    return (color != NULL);  // uint8_t automatically ensures 0-255 range
}

/**
 * @brief Initialize config structure with safe defaults.
 * @details Sets all fields to safe default values.
 * @example
 *     Config cfg;
 *     config_init_defaults(&cfg);
 */
void config_init_defaults(Config *config);

/**
 * @brief Validate display dimensions.
 * @details Checks if width and height are within supported limits.
 * @example
 *     if (config_validate_display_size(width, height)) { ... }
 */
static inline int config_validate_display_size(uint16_t width, uint16_t height) {
    return (width >= CONFIG_MIN_DISPLAY_SIZE && width <= CONFIG_MAX_DISPLAY_SIZE &&
            height >= CONFIG_MIN_DISPLAY_SIZE && height <= CONFIG_MAX_DISPLAY_SIZE);
}

/**
 * @brief Validate temperature value.
 * @details Checks if temperature is within reasonable sensor range.
 * @example
 *     if (config_validate_temperature(temp)) { ... }
 */
static inline int config_validate_temperature(float temperature) {
    return (temperature >= CONFIG_MIN_TEMP && temperature <= CONFIG_MAX_TEMP);
}

/**
 * @brief Clamp value to valid range.
 * @details Ensures value is within specified bounds.
 * @example
 *     int safe_value = config_clamp_int(value, min, max);
 */
static inline int config_clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Clamp float value to valid range.
 * @details Ensures float value is within specified bounds.
 * @example
 *     float safe_temp = config_clamp_float(temp, CONFIG_MIN_TEMP, CONFIG_MAX_TEMP);
 */
static inline float config_clamp_float(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

#endif // CONFIG_H