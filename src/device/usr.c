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
 * @brief User configuration file parsing implementation.
 * @details Provides INI file parsing for user-defined configuration values.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <errno.h>
#include <ini.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "usr.h"
#include "../srv/cc_conf.h"

// ============================================================================
// Change Tracking for Verbose Logging
// ============================================================================

/**
 * @brief Structure to track INI configuration changes.
 * @details Stores section, key, and value for each parsed INI entry.
 */
typedef struct
{
    char section[64];
    char key[64];
    char value[256];
} ConfigChange;

/**
 * @brief Global array to track all INI changes.
 * @details Maximum 100 config changes tracked (sufficient for current config).
 */
#define MAX_CONFIG_CHANGES 100
static ConfigChange config_changes[MAX_CONFIG_CHANGES];
static int config_change_count = 0;

/**
 * @brief Record a configuration change for verbose logging.
 * @details Stores section, key, and value when INI parser encounters an entry.
 */
static void record_config_change(const char *section, const char *key, const char *value)
{
    if (config_change_count >= MAX_CONFIG_CHANGES)
    {
        return; // Buffer full, skip recording
    }

    ConfigChange *change = &config_changes[config_change_count];
    snprintf(change->section, sizeof(change->section), "%s", section ? section : "unknown");
    snprintf(change->key, sizeof(change->key), "%s", key ? key : "unknown");
    snprintf(change->value, sizeof(change->value), "%s", value ? value : "");
    config_change_count++;
}

/**
 * @brief Log all recorded INI configuration changes.
 * @details Only logs if verbose_logging is enabled and changes were recorded.
 */
static void log_config_changes(void)
{
    extern int verbose_logging;

    if (!verbose_logging || config_change_count == 0)
    {
        return; // No logging needed
    }

    log_message(LOG_INFO, "=== User Configuration Changes (from coolerdash.ini) ===");
    log_message(LOG_INFO, "Found %d customized configuration value%s:",
                config_change_count, config_change_count == 1 ? "" : "s");

    for (int i = 0; i < config_change_count; i++)
    {
        ConfigChange *change = &config_changes[i];
        log_message(LOG_INFO, "  [%s] %s = %s", change->section, change->key, change->value);
    }

    log_message(LOG_INFO, "=== End of Configuration Changes ===");
}

/**
 * @brief Reset change tracking (useful for tests or reloads).
 */
static void reset_config_changes(void)
{
    config_change_count = 0;
    memset(config_changes, 0, sizeof(config_changes));
}

// Forward declarations for static handler functions
static int get_daemon_config(Config *config, const char *name, const char *value);
static int get_paths_config(Config *config, const char *name, const char *value);
static int get_display_config(Config *config, const char *name, const char *value);
static int get_layout_config(Config *config, const char *name, const char *value);
static int get_font_config(Config *config, const char *name, const char *value);
static int get_temperature_config(Config *config, const char *name, const char *value);
static int get_color_config(Config *config, const char *section, const char *name, const char *value);
static int get_display_positioning_config(Config *config, const char *name, const char *value);

/**
 * @brief Helper functions for string parsing with validation.
 */
static inline int safe_atoi(const char *str, int default_value)
{
    if (!str || !str[0])
        return default_value;
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0')
        return default_value;
    if (val < INT_MIN || val > INT_MAX)
        return default_value;
    return (int)val;
}

static inline float safe_atof(const char *str, float default_value)
{
    if (!str || !str[0])
        return default_value;
    char *endptr;
    float val = strtof(str, &endptr);
    if (endptr == str || *endptr != '\0')
        return default_value;
    return val;
}

static void parse_color_component(const char *value, uint8_t *component)
{
    if (!value || !component)
        return;
    int val = safe_atoi(value, 0);
    *component = (val < 0) ? 0 : (val > 255) ? 255
                                             : (uint8_t)val;
}

static inline int is_color_section(const char *section)
{
    return (strstr(section, "color") != NULL || strstr(section, "threshold") != NULL);
}

// ============================================================================
// String Configuration Helpers
// ============================================================================

typedef struct
{
    const char *key;
    size_t offset;
    size_t size;
} StringConfigEntry;

/**
 * @brief Generic helper for string-based configuration sections.
 */
static int handle_string_config(Config *config, const char *name, const char *value,
                                const StringConfigEntry *entries, size_t entry_count)
{
    if (!value || value[0] == '\0')
        return 1;

    for (size_t i = 0; i < entry_count; i++)
    {
        if (strcmp(name, entries[i].key) == 0)
        {
            char *dest = (char *)config + entries[i].offset;
            cc_safe_strcpy(dest, entries[i].size, value);
            return 1;
        }
    }
    return 1;
}

// ============================================================================
// Daemon Section Handler
// ============================================================================

static int get_daemon_config(Config *config, const char *name, const char *value)
{
    static const StringConfigEntry entries[] = {
        {"address", offsetof(Config, daemon_address), sizeof(config->daemon_address)},
        {"password", offsetof(Config, daemon_password), sizeof(config->daemon_password)}};

    return handle_string_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

// ============================================================================
// Paths Section Handler
// ============================================================================

static int get_paths_config(Config *config, const char *name, const char *value)
{
    static const StringConfigEntry entries[] = {
        {"images", offsetof(Config, paths_images), sizeof(config->paths_images)},
        {"image_coolerdash", offsetof(Config, paths_image_coolerdash), sizeof(config->paths_image_coolerdash)},
        {"image_shutdown", offsetof(Config, paths_image_shutdown), sizeof(config->paths_image_shutdown)},
        {"pid", offsetof(Config, paths_pid), sizeof(config->paths_pid)}};

    return handle_string_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

// ============================================================================
// Display Section Handlers
// ============================================================================

typedef void (*DisplayConfigHandler)(Config *config, const char *value);

static void handle_display_width(Config *config, const char *value)
{
    int width = safe_atoi(value, 0);
    config->display_width = (width > 0) ? (uint16_t)width : 0;
}

static void handle_display_height(Config *config, const char *value)
{
    int height = safe_atoi(value, 0);
    config->display_height = (height > 0) ? (uint16_t)height : 0;
}

static void handle_refresh_interval_sec(Config *config, const char *value)
{
    config->display_refresh_interval_sec = safe_atoi(value, 0);
}

static void handle_refresh_interval_nsec(Config *config, const char *value)
{
    config->display_refresh_interval_nsec = safe_atoi(value, 0);
}

static void handle_brightness(Config *config, const char *value)
{
    int brightness = safe_atoi(value, 0);
    config->lcd_brightness = (brightness >= 0 && brightness <= 100) ? (uint8_t)brightness : 0;
}

static void handle_orientation(Config *config, const char *value)
{
    int orientation = safe_atoi(value, 0);
    config->lcd_orientation = is_valid_orientation(orientation) ? orientation : 0;
}

typedef struct
{
    const char *key;
    DisplayConfigHandler handler;
} DisplayConfigEntry;

static int get_display_config(Config *config, const char *name, const char *value)
{
    static const DisplayConfigEntry entries[] = {
        {"width", handle_display_width},
        {"height", handle_display_height},
        {"refresh_interval_sec", handle_refresh_interval_sec},
        {"refresh_interval_nsec", handle_refresh_interval_nsec},
        {"brightness", handle_brightness},
        {"orientation", handle_orientation}};

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        if (strcmp(name, entries[i].key) == 0)
        {
            entries[i].handler(config, value);
            return 1;
        }
    }
    return 1;
}

// ============================================================================
// Mixed-Type Configuration Helpers
// ============================================================================

typedef struct
{
    const char *key;
    size_t offset;
    enum
    {
        TYPE_UINT16,
        TYPE_FLOAT,
        TYPE_STRING
    } type;
    size_t string_size;
} MixedConfigEntry;

static int handle_mixed_config(Config *config, const char *name, const char *value,
                               const MixedConfigEntry *entries, size_t entry_count)
{
    for (size_t i = 0; i < entry_count; i++)
    {
        if (strcmp(name, entries[i].key) == 0)
        {
            void *field_ptr = (void *)((char *)config + entries[i].offset);

            switch (entries[i].type)
            {
            case TYPE_UINT16:
            {
                uint16_t *dest = (uint16_t *)field_ptr;
                *dest = (uint16_t)safe_atoi(value, 0);
                break;
            }
            case TYPE_FLOAT:
            {
                float *dest = (float *)field_ptr;
                *dest = safe_atof(value, 12.0f);
                break;
            }
            case TYPE_STRING:
            {
                if (value && value[0] != '\0')
                {
                    char *dest = (char *)field_ptr;
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

// ============================================================================
// Layout Section Handler
// ============================================================================

static int get_layout_config(Config *config, const char *name, const char *value)
{
    static const MixedConfigEntry entries[] = {
        {"bar_height", offsetof(Config, layout_bar_height), TYPE_UINT16, 0},
        {"bar_gap", offsetof(Config, layout_bar_gap), TYPE_UINT16, 0},
        {"bar_border_width", offsetof(Config, layout_bar_border_width), TYPE_FLOAT, 0}};

    return handle_mixed_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

// ============================================================================
// Font Section Handler
// ============================================================================

static int get_font_config(Config *config, const char *name, const char *value)
{
    static const MixedConfigEntry entries[] = {
        {"font_face", offsetof(Config, font_face), TYPE_STRING, CONFIG_MAX_FONT_NAME_LEN},
        {"font_size_temp", offsetof(Config, font_size_temp), TYPE_FLOAT, 0},
        {"font_size_labels", offsetof(Config, font_size_labels), TYPE_FLOAT, 0}};

    return handle_mixed_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

// ============================================================================
// Display Positioning Section Handler
// ============================================================================

typedef struct
{
    const char *key;
    size_t offset;
} PositioningConfigEntry;

static int get_display_positioning_config(Config *config, const char *name, const char *value)
{
    if (!value || value[0] == '\0')
        return 1;

    static const PositioningConfigEntry entries[] = {
        {"display_temp_offset_x", offsetof(Config, display_temp_offset_x)},
        {"display_temp_offset_y", offsetof(Config, display_temp_offset_y)},
        {"display_degree_offset_x", offsetof(Config, display_degree_offset_x)},
        {"display_degree_offset_y", offsetof(Config, display_degree_offset_y)},
        {"display_label_offset_x", offsetof(Config, display_label_offset_x)},
        {"display_label_offset_y", offsetof(Config, display_label_offset_y)}};

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        if (strcmp(name, entries[i].key) == 0)
        {
            void *field_ptr = (void *)((char *)config + entries[i].offset);
            int *dest = (int *)field_ptr;
            *dest = safe_atoi(value, -9999);
            return 1;
        }
    }

    return 1;
}

// ============================================================================
// Temperature Section Handler
// ============================================================================

typedef struct
{
    const char *key;
    size_t offset;
} TemperatureConfigEntry;

static int get_temperature_config(Config *config, const char *name, const char *value)
{
    static const TemperatureConfigEntry entries[] = {
        {"temp_threshold_1", offsetof(Config, temp_threshold_1)},
        {"temp_threshold_2", offsetof(Config, temp_threshold_2)},
        {"temp_threshold_3", offsetof(Config, temp_threshold_3)},
        {"temp_max_scale", offsetof(Config, temp_max_scale)}};

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        if (strcmp(name, entries[i].key) == 0)
        {
            void *field_ptr = (void *)((char *)config + entries[i].offset);
            float *dest = (float *)field_ptr;
            *dest = safe_atof(value, 50.0f + i * 15.0f);
            return 1;
        }
    }
    return 1;
}

// ============================================================================
// Color Configuration Helpers
// ============================================================================

typedef struct
{
    const char *section_name;
    size_t color_offset;
} ColorSectionEntry;

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
        {"temp_threshold_4_bar", offsetof(Config, temp_threshold_4_bar)}};

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        if (strcmp(section, entries[i].section_name) == 0)
        {
            return (Color *)((char *)config + entries[i].color_offset);
        }
    }
    return NULL;
}

static void set_color_component(Color *color, const char *name, const char *value)
{
    if (!color || !name || !value || name[1] != '\0')
        return;

    switch (name[0])
    {
    case 'r':
        parse_color_component(value, &color->r);
        break;
    case 'g':
        parse_color_component(value, &color->g);
        break;
    case 'b':
        parse_color_component(value, &color->b);
        break;
    }
}

static int get_color_config(Config *config, const char *section, const char *name, const char *value)
{
    Color *color = get_color_pointer_from_section(config, section);
    if (color)
    {
        set_color_component(color, name, value);
    }
    return 1;
}

// ============================================================================
// Main INI Parser Callback
// ============================================================================

typedef int (*SectionHandler)(Config *config, const char *name, const char *value);
typedef int (*ColorSectionHandler)(Config *config, const char *section, const char *name, const char *value);

typedef struct
{
    const char *section_name;
    SectionHandler handler;
} SectionHandlerEntry;

static int parse_config_data(void *user, const char *section, const char *name, const char *value)
{
    Config *config = (Config *)user;

    // Record this configuration change for verbose logging
    record_config_change(section, name, value);

    static const SectionHandlerEntry handlers[] = {
        {"daemon", get_daemon_config},
        {"paths", get_paths_config},
        {"display", get_display_config},
        {"layout", get_layout_config},
        {"font", get_font_config},
        {"display_positioning", get_display_positioning_config},
        {"temperature", get_temperature_config}};

    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++)
    {
        if (strcmp(section, handlers[i].section_name) == 0)
        {
            return handlers[i].handler(config, name, value);
        }
    }

    if (is_color_section(section))
    {
        return get_color_config(config, section, name, value);
    }

    return 1;
}

// ============================================================================
// Public API Implementation
// ============================================================================

int load_user_config(const char *path, Config *config)
{
    if (!config || !path)
    {
        return -1;
    }

    // Reset change tracking before parsing
    reset_config_changes();

    // Check if file exists and is readable
    FILE *file = fopen(path, "r");
    if (!file)
    {
        // File doesn't exist - skip user config, system defaults will be used
        log_message(LOG_INFO, "User config file '%s' not found, using system defaults", path);
        return 0; // Not an error - missing user config is valid
    }
    fclose(file);

    // Parse INI file and return success/failure
    int result = (ini_parse(path, parse_config_data, config) < 0) ? -1 : 0;

    if (result < 0)
    {
        log_message(LOG_ERROR, "Failed to parse config file '%s'", path);
    }
    else
    {
        log_message(LOG_INFO, "User configuration loaded from '%s'", path);

        // Log all configuration changes if verbose logging is enabled
        log_config_changes();
    }

    return result;
}
