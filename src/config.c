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
 * @brief String configuration entry for lookup table.
 * @details Structure for string configuration keys.
 */
typedef struct {
    const char *key;
    size_t offset;
    size_t size;
} StringConfigEntry;

/**
 * @brief Generic helper for string-based configuration sections.
 * @details Processes string configuration keys using lookup table approach.
 */
static int handle_string_config(Config *config, const char *name, const char *value, 
                               const StringConfigEntry *entries, size_t entry_count)
{
    if (!value || value[0] == '\0') return 1;
    
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(name, entries[i].key) == 0) {
            char *dest = (char*)config + entries[i].offset;
            cc_safe_strcpy(dest, entries[i].size, value);
            return 1;
        }
    }
    return 1;
}

/**
 * @brief Handle daemon section configuration.
 * @details Processes daemon-related configuration keys using lookup table approach.
 */
static int get_daemon_config(Config *config, const char *name, const char *value)
{
    static const StringConfigEntry entries[] = {
        {"address", offsetof(Config, daemon_address), sizeof(config->daemon_address)},
        {"password", offsetof(Config, daemon_password), sizeof(config->daemon_password)}
    };
    
    return handle_string_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

/**
 * @brief Handle paths section configuration with reduced complexity.
 * @details Processes path-related configuration keys using lookup table approach.
 */
static int get_paths_config(Config *config, const char *name, const char *value)
{
    static const StringConfigEntry entries[] = {
        {"images", offsetof(Config, paths_images), sizeof(config->paths_images)},
        {"image_coolerdash", offsetof(Config, paths_image_coolerdash), sizeof(config->paths_image_coolerdash)},
        {"image_shutdown", offsetof(Config, paths_image_shutdown), sizeof(config->paths_image_shutdown)},
        {"pid", offsetof(Config, paths_pid), sizeof(config->paths_pid)}
    };

    return handle_string_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

/**
 * @brief Validate orientation value.
 * @details Helper function to validate LCD orientation values.
 */
static inline int is_valid_orientation(int orientation)
{
    return (orientation == 0 || orientation == 90 || orientation == 180);
}

/**
 * @brief Display configuration handler function type.
 * @details Function pointer type for individual display configuration handlers.
 */
typedef void (*DisplayConfigHandler)(Config *config, const char *value);

/**
 * @brief Display configuration handlers.
 * @details Individual handlers for each display configuration key.
 */
static void handle_display_width(Config *config, const char *value) {
    int width = safe_atoi(value, 0);
    config->display_width = (width > 0) ? (uint16_t)width : 0;
}

static void handle_display_height(Config *config, const char *value) {
    int height = safe_atoi(value, 0);
    config->display_height = (height > 0) ? (uint16_t)height : 0;
}

static void handle_refresh_interval_sec(Config *config, const char *value) {
    config->display_refresh_interval_sec = safe_atoi(value, 0);
}

static void handle_refresh_interval_nsec(Config *config, const char *value) {
    config->display_refresh_interval_nsec = safe_atoi(value, 0);
}

static void handle_brightness(Config *config, const char *value) {
    int brightness = safe_atoi(value, 0);
    config->lcd_brightness = (brightness >= 0 && brightness <= 100) ? (uint8_t)brightness : 0;
}

static void handle_orientation(Config *config, const char *value) {
    int orientation = safe_atoi(value, 0);
    config->lcd_orientation = is_valid_orientation(orientation) ? orientation : 0;
}

/**
 * @brief Display configuration entry for lookup table.
 */
typedef struct {
    const char *key;
    DisplayConfigHandler handler;
} DisplayConfigEntry;

/**
 * @brief Handle display section configuration with reduced complexity.
 * @details Processes display-related configuration keys using lookup table approach.
 */
static int get_display_config(Config *config, const char *name, const char *value)
{
    static const DisplayConfigEntry entries[] = {
        {"width", handle_display_width},
        {"height", handle_display_height},
        {"refresh_interval_sec", handle_refresh_interval_sec},
        {"refresh_interval_nsec", handle_refresh_interval_nsec},
        {"brightness", handle_brightness},
        {"orientation", handle_orientation}
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (strcmp(name, entries[i].key) == 0) {
            entries[i].handler(config, value);
            return 1;
        }
    }
    return 1;
}

/**
 * @brief Mixed-type configuration entry for lookup table.
 */
typedef struct {
    const char *key;
    size_t offset;
    enum { TYPE_UINT16, TYPE_FLOAT, TYPE_STRING } type;
    size_t string_size;
} MixedConfigEntry;

/**
 * @brief Generic helper for mixed-type configuration sections.
 * @details Processes mixed-type configuration keys using lookup table approach.
 */
static int handle_mixed_config(Config *config, const char *name, const char *value,
                              const MixedConfigEntry *entries, size_t entry_count)
{
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(name, entries[i].key) == 0) {
            void *field_ptr = (void*)((char*)config + entries[i].offset);
            
            switch (entries[i].type) {
                case TYPE_UINT16: {
                    uint16_t *dest = (uint16_t*)field_ptr;
                    *dest = (uint16_t)safe_atoi(value, 0);
                    break;
                }
                case TYPE_FLOAT: {
                    float *dest = (float*)field_ptr;
                    *dest = safe_atof(value, 12.0f);
                    break;
                }
                case TYPE_STRING: {
                    if (value && value[0] != '\0') {
                        char *dest = (char*)field_ptr;
                        cc_safe_strcpy(dest, entries[i].string_size, value);
                    }
                    break;
                }
            }
            return 1;
        }
    }
    return 1;
}

/**
 * @brief Handle layout section configuration with reduced complexity.
 * @details Processes layout-related configuration keys using lookup table approach.
 */
static int get_layout_config(Config *config, const char *name, const char *value)
{
    static const MixedConfigEntry entries[] = {
        {"box_width", offsetof(Config, layout_box_width), TYPE_UINT16, 0},
        {"box_height", offsetof(Config, layout_box_height), TYPE_UINT16, 0},
        {"box_gap", offsetof(Config, layout_box_gap), TYPE_UINT16, 0},
        {"bar_width", offsetof(Config, layout_bar_width), TYPE_UINT16, 0},
        {"bar_height", offsetof(Config, layout_bar_height), TYPE_UINT16, 0},
        {"bar_gap", offsetof(Config, layout_bar_gap), TYPE_UINT16, 0},
        {"bar_border_width", offsetof(Config, layout_bar_border_width), TYPE_FLOAT, 0}
    };

    return handle_mixed_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

/**
 * @brief Handle font section configuration with reduced complexity.
 * @details Processes font-related configuration keys using lookup table approach.
 */
static int get_font_config(Config *config, const char *name, const char *value)
{
    static const MixedConfigEntry entries[] = {
        {"font_face", offsetof(Config, font_face), TYPE_STRING, CONFIG_MAX_FONT_NAME_LEN},
        {"font_size_temp", offsetof(Config, font_size_temp), TYPE_FLOAT, 0},
        {"font_size_labels", offsetof(Config, font_size_labels), TYPE_FLOAT, 0}
    };

    return handle_mixed_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

/**
 * @brief Temperature configuration entry for lookup table.
 * @details Structure for temperature threshold configuration keys.
 */
typedef struct {
    const char *key;
    size_t offset;
} TemperatureConfigEntry;

/**
 * @brief Handle temperature section configuration with reduced complexity.
 * @details Processes temperature threshold configuration keys using lookup table approach.
 */
static int get_temperature_config(Config *config, const char *name, const char *value)
{
    static const TemperatureConfigEntry entries[] = {
        {"temp_threshold_1", offsetof(Config, temp_threshold_1)},
        {"temp_threshold_2", offsetof(Config, temp_threshold_2)},
        {"temp_threshold_3", offsetof(Config, temp_threshold_3)}
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (strcmp(name, entries[i].key) == 0) {
            void *field_ptr = (void*)((char*)config + entries[i].offset);
            float *dest = (float*)field_ptr;
            *dest = safe_atof(value, 50.0f + i * 15.0f); // 50, 65, 80
            return 1;
        }
    }
    return 1;
}

/**
 * @brief Color section mapping entry for lookup table.
 * @details Structure for color section mapping.
 */
typedef struct {
    const char *section_name;
    size_t color_offset;
} ColorSectionEntry;

/**
 * @brief Color configuration mapping using lookup table.
 * @details Uses lookup table to reduce cyclomatic complexity.
 */
static Color *get_color_pointer_from_section(Config *config, const char *section)
{
    static const ColorSectionEntry entries[] = {
        {"bar_color_background", offsetof(Config, layout_bar_color_background)},
        {"bar_color_border", offsetof(Config, layout_bar_color_border)},
        {"font_color_temp", offsetof(Config, font_color_temp)},
        {"font_color_label", offsetof(Config, font_color_label)},
        {"temp_threshold_1_bar", offsetof(Config, temp_threshold_1_bar)},
        {"temp_threshold_2_bar", offsetof(Config, temp_threshold_2_bar)},
        {"temp_threshold_3_bar", offsetof(Config, temp_threshold_3_bar)},
        {"temp_threshold_4_bar", offsetof(Config, temp_threshold_4_bar)}
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (strcmp(section, entries[i].section_name) == 0) {
            return (Color*)((char*)config + entries[i].color_offset);
        }
    }
    return NULL;
}

/**
 * @brief Set color component using lookup approach.
 * @details Helper function to set R, G, or B component with reduced complexity.
 */
static void set_color_component(Color *color, const char *name, const char *value)
{
    if (!color || !name || !value) return;

    switch (name[0]) {
        case 'r': if (strcmp(name, "r") == 0) parse_color_component(value, &color->r); break;
        case 'g': if (strcmp(name, "g") == 0) parse_color_component(value, &color->g); break;
        case 'b': if (strcmp(name, "b") == 0) parse_color_component(value, &color->b); break;
    }
}

/**
 * @brief Handle color section configuration with reduced complexity.
 * @details Processes color-related configuration keys using lookup table approach.
 */
static int get_color_config(Config *config, const char *section, const char *name, const char *value)
{
    Color *color = get_color_pointer_from_section(config, section);
    if (color) {
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
 * @brief Color default configuration entry.
 * @details Structure for color default values lookup table.
 */
typedef struct
{
    Color *color_ptr;
    uint8_t r, g, b;
} ColorDefault;

/**
 * @brief Set color default values using lookup table approach.
 * @details Helper function to set default color values using data-driven approach to reduce complexity.
 */
static void set_color_defaults(Config *config)
{
    // Define all color defaults in a lookup table
    ColorDefault color_defaults[] = {
        {&config->layout_bar_color_background, 52, 52, 52},
        {&config->layout_bar_color_border, 192, 192, 192},
        {&config->font_color_temp, 255, 255, 255},
        {&config->font_color_label, 200, 200, 200},
        {&config->temp_threshold_1_bar, 0, 255, 0},
        {&config->temp_threshold_2_bar, 255, 140, 0},
        {&config->temp_threshold_3_bar, 255, 70, 0},
        {&config->temp_threshold_4_bar, 255, 0, 0}};

    const size_t color_count = sizeof(color_defaults) / sizeof(color_defaults[0]);
    for (size_t i = 0; i < color_count; i++)
    {
        if (is_color_unset(color_defaults[i].color_ptr))
        {
            color_defaults[i].color_ptr->r = color_defaults[i].r;
            color_defaults[i].color_ptr->g = color_defaults[i].g;
            color_defaults[i].color_ptr->b = color_defaults[i].b;
        }
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