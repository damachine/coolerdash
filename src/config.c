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
 * @brief INI parser handler for CoolerDash configuration.
 * @details Parses the configuration file and sets values in the Config struct.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <errno.h>
#include <ini.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"

/**
 * @brief Global logging implementation for all modules except main.c
 * @details Provides unified log output for info, status, warning and error messages.
 */
void log_message(log_level_t level, const char *format, ...)
{
    if (level == LOG_INFO && !verbose_logging)
    {
        return;
    }
    const char *prefix[] = {"INFO", "STATUS", "WARNING", "ERROR"};
    FILE *output = (level == LOG_ERROR) ? stderr : stdout;
    fprintf(output, "[CoolerDash %s] ", prefix[level]);

    enum
    {
        LOG_MSG_CAP = 1024
    };
    char msg_buf[LOG_MSG_CAP];
    msg_buf[0] = '\0';
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buf, sizeof(msg_buf), (format ? format : "(null)"), args);
    va_end(args);
    fputs(msg_buf, output);
    fputc('\n', output);
    fflush(output);
}

// Helper macro for safe string copying using project helper (guarantees termination & bounds)
#define SAFE_STRCPY(dest, src)                       \
    do                                               \
    {                                                \
        cc_safe_strcpy((dest), sizeof(dest), (src)); \
    } while (0)

// Forward declarations for static handler functions
static int get_daemon_config(Config *config, const char *name, const char *value);
static int get_paths_config(Config *config, const char *name, const char *value);
static int get_display_config(Config *config, const char *name, const char *value);
static int get_layout_config(Config *config, const char *name, const char *value);
static int get_font_config(Config *config, const char *name, const char *value);
static int get_temperature_config(Config *config, const char *name, const char *value);
static int get_color_config(Config *config, const char *section, const char *name, const char *value);

// Logging erfolgt zentral über log_message aus config.h

/**
 * @brief Helper functions for string parsing with validation.
 * @details Replaces atoi with secure parsing that validates input and handles overflow conditions.
 */
static inline int safe_atoi(const char *str, int default_value)
{
    if (!str || !str[0])
        return default_value;
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0')
        return default_value; // Invalid conversion
    if (val < INT_MIN || val > INT_MAX)
        return default_value; // Overflow
    return (int)val;
}

/**
 * @brief Helper functions for floating-point parsing with validation.
 * @details Replaces atof with a safer version that validates input and handles overflow conditions.
 */
static inline float safe_atof(const char *str, float default_value)
{
    if (!str || !str[0])
        return default_value;
    char *endptr;
    float val = strtof(str, &endptr);
    if (endptr == str || *endptr != '\0')
        return default_value; // Invalid conversion
    return val;
}

/**
 * @brief Helper function for secure color parsing with validation.
 * @details Parses RGB color values and automatically validates with uint8_t type safety, clamping values to 0-255 range.
 */
static void parse_color_component(const char *value, uint8_t *component)
{
    if (!value || !component)
        return;
    int val = safe_atoi(value, 0);
    *component = (val < 0) ? 0 : (val > 255) ? 255
                                             : (uint8_t)val;
}

/**
 * @brief Check if section matches color pattern.
 * @details Helper function to identify color-related sections.
 */
static inline int is_color_section(const char *section)
{
    return (strstr(section, "color") != NULL || strstr(section, "_bar") != NULL);
}

/**
 * @brief Main INI parser handler with reduced complexity.
 * @details Simplified parser using early returns to reduce nesting and improve readability.
 */
static int parse_config_data(void *user, const char *section, const char *name, const char *value)
{
    Config *config = (Config *)user;

    // Handle standard sections with early returns
    if (strcmp(section, "daemon") == 0)
    {
        return get_daemon_config(config, name, value);
    }
    if (strcmp(section, "paths") == 0)
    {
        return get_paths_config(config, name, value);
    }
    if (strcmp(section, "display") == 0)
    {
        return get_display_config(config, name, value);
    }
    if (strcmp(section, "layout") == 0)
    {
        return get_layout_config(config, name, value);
    }
    if (strcmp(section, "font") == 0)
    {
        return get_font_config(config, name, value);
    }
    if (strcmp(section, "temperature") == 0)
    {
        return get_temperature_config(config, name, value);
    }

    // Handle color sections
    if (is_color_section(section))
    {
        return get_color_config(config, section, name, value);
    }

    return 1;
}

/**
 * @brief Handle daemon section configuration.
 * @details Processes daemon-related configuration keys (address, password) with safe string copying.
 */
static int get_daemon_config(Config *config, const char *name, const char *value)
{
    if (strcmp(name, "address") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->daemon_address, value);
        }
    }
    else if (strcmp(name, "password") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->daemon_password, value);
        }
    }
    return 1;
}

/**
 * @brief Handle paths section configuration.
 * @details Processes path-related configuration keys (images, pid, etc.) with validation and safe string operations.
 */
static int get_paths_config(Config *config, const char *name, const char *value)
{
    if (strcmp(name, "images") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->paths_images, value);
        }
    }
    else if (strcmp(name, "image_coolerdash") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->paths_image_coolerdash, value);
        }
    }
    else if (strcmp(name, "image_shutdown") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->paths_image_shutdown, value);
        }
    }
    else if (strcmp(name, "pid") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->paths_pid, value);
        }
    }
    return 1;
}

/**
 * @brief Handle display section configuration.
 * @details Processes display-related configuration keys (width, height, brightness, etc.) with range validation and type safety.
 */
static int get_display_config(Config *config, const char *name, const char *value)
{
    if (strcmp(name, "width") == 0)
    {
        int width = safe_atoi(value, 0);
        config->display_width = (width > 0) ? (uint16_t)width : 0;
    }
    else if (strcmp(name, "height") == 0)
    {
        int height = safe_atoi(value, 0);
        config->display_height = (height > 0) ? (uint16_t)height : 0;
    }
    else if (strcmp(name, "refresh_interval_sec") == 0)
    {
        config->display_refresh_interval_sec = safe_atoi(value, 0);
    }
    else if (strcmp(name, "refresh_interval_nsec") == 0)
    {
        config->display_refresh_interval_nsec = safe_atoi(value, 0);
    }
    else if (strcmp(name, "brightness") == 0)
    {
        int brightness = safe_atoi(value, 0);
        config->lcd_brightness = (brightness >= 0 && brightness <= 100) ? (uint8_t)brightness : 0;
    }
    else if (strcmp(name, "orientation") == 0)
    {
        int orientation = safe_atoi(value, 0);
        // Only allow 0, 90, 180 degrees (KISS: simple validation)
        if (orientation == 0 || orientation == 90 || orientation == 180)
        {
            config->lcd_orientation = orientation;
        }
        else
        {
            config->lcd_orientation = 0; // Default to 0 for invalid values
        }
    }
    return 1;
}

/**
 * @brief Handle layout section configuration.
 * @details Processes layout-related configuration keys (box dimensions, bar settings, etc.) with numeric validation.
 */
static int get_layout_config(Config *config, const char *name, const char *value)
{
    if (strcmp(name, "box_width") == 0)
    {
        config->layout_box_width = safe_atoi(value, 0);
    }
    else if (strcmp(name, "box_height") == 0)
    {
        config->layout_box_height = safe_atoi(value, 0);
    }
    else if (strcmp(name, "box_gap") == 0)
    {
        config->layout_box_gap = safe_atoi(value, 0);
    }
    else if (strcmp(name, "bar_width") == 0)
    {
        config->layout_bar_width = safe_atoi(value, 0);
    }
    else if (strcmp(name, "bar_height") == 0)
    {
        config->layout_bar_height = safe_atoi(value, 0);
    }
    else if (strcmp(name, "bar_gap") == 0)
    {
        config->layout_bar_gap = safe_atoi(value, 0);
    }
    else if (strcmp(name, "bar_border_width") == 0)
    {
        config->layout_bar_border_width = safe_atof(value, 0.0f);
    }
    return 1;
}

/**
 * @brief Handle font section configuration.
 * @details Processes font-related configuration keys (face, sizes) with string and float validation.
 */
static int get_font_config(Config *config, const char *name, const char *value)
{
    if (strcmp(name, "font_face") == 0)
    {
        if (value && value[0] != '\0')
        {
            SAFE_STRCPY(config->font_face, value);
        }
    }
    else if (strcmp(name, "font_size_temp") == 0)
    {
        config->font_size_temp = safe_atof(value, 12.0f);
    }
    else if (strcmp(name, "font_size_labels") == 0)
    {
        config->font_size_labels = safe_atof(value, 10.0f);
    }
    return 1;
}

/**
 * @brief Handle temperature section configuration.
 * @details Processes temperature threshold configuration keys with float validation and safe parsing.
 */
static int get_temperature_config(Config *config, const char *name, const char *value)
{
    if (strcmp(name, "temp_threshold_1") == 0)
    {
        config->temp_threshold_1 = safe_atof(value, 50.0f);
    }
    else if (strcmp(name, "temp_threshold_2") == 0)
    {
        config->temp_threshold_2 = safe_atof(value, 65.0f);
    }
    else if (strcmp(name, "temp_threshold_3") == 0)
    {
        config->temp_threshold_3 = safe_atof(value, 80.0f);
    }
    return 1;
}

/**
 * @brief Color configuration mapping with offset approach.
 * @details Lookup table using string comparison to reduce cyclomatic complexity.
 */
static Color *get_color_pointer_from_section(Config *config, const char *section)
{
    if (strcmp(section, "bar_color_background") == 0)
    {
        return &config->layout_bar_color_background;
    }
    else if (strcmp(section, "bar_color_border") == 0)
    {
        return &config->layout_bar_color_border;
    }
    else if (strcmp(section, "font_color_temp") == 0)
    {
        return &config->font_color_temp;
    }
    else if (strcmp(section, "font_color_label") == 0)
    {
        return &config->font_color_label;
    }
    else if (strcmp(section, "temp_threshold_1_bar") == 0)
    {
        return &config->temp_threshold_1_bar;
    }
    else if (strcmp(section, "temp_threshold_2_bar") == 0)
    {
        return &config->temp_threshold_2_bar;
    }
    else if (strcmp(section, "temp_threshold_3_bar") == 0)
    {
        return &config->temp_threshold_3_bar;
    }
    else if (strcmp(section, "temp_threshold_4_bar") == 0)
    {
        return &config->temp_threshold_4_bar;
    }
    return NULL;
}

/**
 * @brief Set color component based on name.
 * @details Helper function to set R, G, or B component of a color structure.
 */
static void set_color_component(Color *color, const char *name, const char *value)
{
    if (!color || !name || !value)
        return;

    if (strcmp(name, "r") == 0)
    {
        parse_color_component(value, &color->r);
    }
    else if (strcmp(name, "g") == 0)
    {
        parse_color_component(value, &color->g);
    }
    else if (strcmp(name, "b") == 0)
    {
        parse_color_component(value, &color->b);
    }
}

/**
 * @brief Handle color section configuration with reduced complexity.
 * @details Processes color-related configuration keys using lookup table approach to eliminate deep nesting.
 */
static int get_color_config(Config *config, const char *section, const char *name, const char *value)
{
    Color *color = get_color_pointer_from_section(config, section);
    if (color)
    {
        set_color_component(color, name, value);
    }
    return 1;
}

/**
 * @brief Set daemon default values.
 * @details Helper function to set default daemon configuration values.
 */
static void set_daemon_defaults(Config *config)
{
    if (config->daemon_address[0] == '\0')
    {
        SAFE_STRCPY(config->daemon_address, "http://localhost:11987");
    }
    if (config->daemon_password[0] == '\0')
    {
        SAFE_STRCPY(config->daemon_password, "coolAdmin");
    }
}

/**
 * @brief Set paths default values.
 * @details Helper function to set default path configuration values.
 */
static void set_paths_defaults(Config *config)
{
    if (config->paths_images[0] == '\0')
    {
        SAFE_STRCPY(config->paths_images, "/opt/coolerdash/images");
    }
    if (config->paths_image_coolerdash[0] == '\0')
    {
        SAFE_STRCPY(config->paths_image_coolerdash, "/tmp/coolerdash.png");
    }
    if (config->paths_image_shutdown[0] == '\0')
    {
        SAFE_STRCPY(config->paths_image_shutdown, "/opt/coolerdash/images/shutdown.png");
    }
    if (config->paths_pid[0] == '\0')
    {
        SAFE_STRCPY(config->paths_pid, "/tmp/coolerdash.pid");
    }
}

/**
 * @brief Set display default values with LCD device fallback.
 * @details Helper function to set default display configuration values, attempting to get LCD dimensions from device.
 */
static void set_display_defaults(Config *config)
{
    // Try to get dimensions from Liquidctl device first
    if (config->display_width == 0 || config->display_height == 0)
    {
        int lcd_width = 0, lcd_height = 0;
        if (get_liquidctl_data(config, NULL, 0, NULL, 0, &lcd_width, &lcd_height) &&
            lcd_width > 0 && lcd_height > 0)
        {
            if (config->display_width == 0)
                config->display_width = lcd_width;
            if (config->display_height == 0)
                config->display_height = lcd_height;
        }
    }

    if (config->display_refresh_interval_sec == 0)
        config->display_refresh_interval_sec = 2;
    if (config->display_refresh_interval_nsec == 0)
        config->display_refresh_interval_nsec = 500000000;
    if (config->lcd_brightness == 0)
        config->lcd_brightness = 80;
    if (config->lcd_orientation == 0)
        config->lcd_orientation = 0;
}

/**
 * @brief Set layout default values.
 * @details Helper function to set default layout configuration values based on display dimensions.
 */
static void set_layout_defaults(Config *config)
{
    if (config->layout_box_width == 0)
        config->layout_box_width = config->display_width;
    if (config->layout_box_height == 0)
        config->layout_box_height = config->display_height / 2;
    if (config->layout_bar_width == 0)
        config->layout_bar_width = config->layout_box_width - 10;
    if (config->layout_bar_height == 0)
        config->layout_bar_height = 22;
    if (config->layout_bar_gap == 0)
        config->layout_bar_gap = 10;
    if (config->layout_bar_border_width == 0.0f)
        config->layout_bar_border_width = 1.5f;
}

/**
 * @brief Set font default values.
 * @details Helper function to set default font configuration values.
 */
static void set_font_defaults(Config *config)
{
    if (config->font_face[0] == '\0')
        SAFE_STRCPY(config->font_face, "Roboto Black");
    if (config->font_size_temp == 0.0f)
        config->font_size_temp = 100.0f;
    if (config->font_size_labels == 0.0f)
        config->font_size_labels = 30.0f;
}

/**
 * @brief Set temperature threshold default values.
 * @details Helper function to set default temperature threshold values.
 */
static void set_temperature_defaults(Config *config)
{
    if (config->temp_threshold_1 == 0.0f)
        config->temp_threshold_1 = 55.0f;
    if (config->temp_threshold_2 == 0.0f)
        config->temp_threshold_2 = 65.0f;
    if (config->temp_threshold_3 == 0.0f)
        config->temp_threshold_3 = 75.0f;
}

/**
 * @brief Check if color is unset (all components are zero).
 * @details Helper function to determine if a color structure needs default values.
 */
static inline int is_color_unset(const Color *color)
{
    return (color->r == 0 && color->g == 0 && color->b == 0);
}

/**
 * @brief Set color default values using structured approach.
 * @details Helper function to set default color values for all UI elements.
 */
static void set_color_defaults(Config *config)
{
    // Layout colors
    if (is_color_unset(&config->layout_bar_color_background))
    {
        config->layout_bar_color_background.r = 52;
        config->layout_bar_color_background.g = 52;
        config->layout_bar_color_background.b = 52;
    }
    if (is_color_unset(&config->layout_bar_color_border))
    {
        config->layout_bar_color_border.r = 192;
        config->layout_bar_color_border.g = 192;
        config->layout_bar_color_border.b = 192;
    }

    // Font colors
    if (is_color_unset(&config->font_color_temp))
    {
        config->font_color_temp.r = 255;
        config->font_color_temp.g = 255;
        config->font_color_temp.b = 255;
    }
    if (is_color_unset(&config->font_color_label))
    {
        config->font_color_label.r = 200;
        config->font_color_label.g = 200;
        config->font_color_label.b = 200;
    }

    // Temperature bar colors
    if (is_color_unset(&config->temp_threshold_1_bar))
    {
        config->temp_threshold_1_bar.r = 0;
        config->temp_threshold_1_bar.g = 255;
        config->temp_threshold_1_bar.b = 0;
    }
    if (is_color_unset(&config->temp_threshold_2_bar))
    {
        config->temp_threshold_2_bar.r = 255;
        config->temp_threshold_2_bar.g = 140;
        config->temp_threshold_2_bar.b = 0;
    }
    if (is_color_unset(&config->temp_threshold_3_bar))
    {
        config->temp_threshold_3_bar.r = 255;
        config->temp_threshold_3_bar.g = 70;
        config->temp_threshold_3_bar.b = 0;
    }
    if (is_color_unset(&config->temp_threshold_4_bar))
    {
        config->temp_threshold_4_bar.r = 255;
        config->temp_threshold_4_bar.g = 0;
        config->temp_threshold_4_bar.b = 0;
    }
}

/**
 * @brief Sets fallback default values for missing or empty configuration fields.
 * @details Refactored function with reduced complexity using helper functions for each configuration section.
 */
void get_config_defaults(Config *config)
{
    if (!config)
        return;

    set_daemon_defaults(config);
    set_paths_defaults(config);
    set_display_defaults(config);
    set_layout_defaults(config);
    set_font_defaults(config);
    set_temperature_defaults(config);
    set_color_defaults(config);
}

/**
 * @brief Initialize config structure with safe defaults.
 * @details Sets all fields to safe default values with security considerations by clearing memory and applying fallback values.
 */
void init_config_defaults(Config *config)
{
    if (!config)
        return;

    memset(config, 0, sizeof(Config));

    // Apply all fallback values
    get_config_defaults(config);
}

/**
 * @brief Main configuration loading function with enhanced security.
 * @details Loads configuration from INI file with comprehensive validation and secure defaults, handling missing files gracefully.
 */
int load_config(const char *path, Config *config)
{
    // Validate input parameters
    if (!config || !path)
    {
        return -1;
    }

    // Initialize config struct with zeros to ensure fallbacks work
    memset(config, 0, sizeof(Config));

    // Check if file exists and is readable
    FILE *file = fopen(path, "r");
    if (!file)
    {
        // File doesn't exist - use fallbacks only
        log_message(LOG_INFO, "Config file '%s' not found, using fallback values", path);
        get_config_defaults(config);
        return 0; // Return success, fallbacks are valid
    }
    fclose(file);

    // Parse INI file and return success/failure
    int result = (ini_parse(path, parse_config_data, config) < 0) ? -1 : 0;

    // Always apply fallbacks after parsing (for missing/commented values)
    get_config_defaults(config);

    return result;
}