/**
* -----------------------------------------------------------------------------
* Created by: damachine (christkue79 at gmail dot com)
* Website: https://github.com/damachine/coolerdash
* -----------------------------------------------------------------------------
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
 * @details Always logs user customizations to systemd journal (LOG_STATUS level).
 *          This ensures manual config changes are visible without --verbose flag.
 */
static void log_config_changes(void)
{
    if (config_change_count == 0)
    {
        return; // No changes to log
    }

    log_message(LOG_STATUS, "=== User Configuration Changes (from coolerdash.ini) ===");
    log_message(LOG_STATUS, "Found %d customized configuration value%s:",
                config_change_count, config_change_count == 1 ? "" : "s");

    for (int i = 0; i < config_change_count; i++)
    {
        ConfigChange *change = &config_changes[i];
        log_message(LOG_STATUS, "  [%s] %s = %s", change->section, change->key, change->value);
    }

    log_message(LOG_STATUS, "=== End of Configuration Changes ===");
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
    if (endptr == str)
        return default_value;
    // Allow trailing whitespace and comments (INI format)
    while (*endptr == ' ' || *endptr == '\t')
        endptr++;
    if (*endptr != '\0' && *endptr != '#')
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
    if (endptr == str)
        return default_value;
    // Allow trailing whitespace and comments (INI format)
    while (*endptr == ' ' || *endptr == '\t')
        endptr++;
    if (*endptr != '\0' && *endptr != '#')
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

static void handle_refresh_interval(Config *config, const char *value)
{
    float interval = strtof(value, NULL);
    if (interval >= 0.01f && interval <= 60.0f)
    {
        config->display_refresh_interval = interval;
    }
    else
    {
        log_message(LOG_WARNING, "refresh_interval must be 0.01-60.0, using default: 2.50");
        config->display_refresh_interval = 2.50f;
    }
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

static void handle_display_mode(Config *config, const char *value)
{
    if (strcmp(value, "dual") == 0 || strcmp(value, "circle") == 0)
    {
        cc_safe_strcpy(config->display_mode, sizeof(config->display_mode), value);
    }
    else
    {
        log_message(LOG_WARNING, "Invalid display_mode '%s', defaulting to 'dual'", value);
        cc_safe_strcpy(config->display_mode, sizeof(config->display_mode), "dual");
    }
}

static void handle_display_shape(Config *config, const char *value)
{
    if (strcmp(value, "auto") == 0 || strcmp(value, "rectangular") == 0 || strcmp(value, "circular") == 0)
    {
        cc_safe_strcpy(config->display_shape, sizeof(config->display_shape), value);
    }
    else
    {
        log_message(LOG_WARNING, "Invalid display_shape '%s', defaulting to 'auto'", value);
        cc_safe_strcpy(config->display_shape, sizeof(config->display_shape), "auto");
    }
}

static void handle_circle_switch_interval(Config *config, const char *value)
{
    int interval = safe_atoi(value, 5); // Default: 5 seconds
    if (interval < 1)
    {
        log_message(LOG_WARNING, "Circle switch interval must be >= 1 second, using default (5s)");
        config->circle_switch_interval = 5;
    }
    else if (interval > 60)
    {
        log_message(LOG_WARNING, "Circle switch interval too large (%d), capping at 60 seconds", interval);
        config->circle_switch_interval = 60;
    }
    else
    {
        config->circle_switch_interval = interval;
    }
}

static void handle_content_scale_factor(Config *config, const char *value)
{
    float scale = strtof(value, NULL);
    if (scale < 0.5f || scale > 1.0f)
    {
        log_message(LOG_WARNING, "Content scale factor must be between 0.5 and 1.0, using default (0.98)");
        config->display_content_scale_factor = 0.98f;
    }
    else
    {
        config->display_content_scale_factor = scale;
    }
}

static void handle_display_inscribe_factor(Config *config, const char *value)
{
    float val = strtof(value, NULL);
    if (val < 0.0f || val > 1.0f)
    {
        log_message(LOG_WARNING, "display_inscribe_factor must be >=0 && <=1 (0 = auto), using default (%.2f)", config->display_inscribe_factor < 0.0f ? 0.70710678f : config->display_inscribe_factor);
        return;
    }
    // val == 0.0f is allowed and interpreted as 'auto' (use geometric inscribe factor at runtime)
    config->display_inscribe_factor = val;
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
        {"refresh_interval", handle_refresh_interval},
        {"brightness", handle_brightness},
        {"orientation", handle_orientation},
        {"shape", handle_display_shape},
        {"mode", handle_display_mode},
        {"circle_switch_interval", handle_circle_switch_interval},
        {"content_scale_factor", handle_content_scale_factor},
        {"inscribe_factor", handle_display_inscribe_factor}};

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

static void handle_layout_bar_width(Config *config, const char *value)
{
    int val = safe_atoi(value, 98);
    if (val >= 1 && val <= 100)
    {
        config->layout_bar_width = (uint8_t)val;
        record_config_change("layout", "bar_width", value);
    }
}

static void handle_layout_label_margin_left(Config *config, const char *value)
{
    int val = safe_atoi(value, 1);
    if (val >= 1 && val <= 50)
    {
        config->layout_label_margin_left = (uint8_t)val;
        record_config_change("layout", "label_margin_left", value);
    }
}

static void handle_layout_label_margin_bar(Config *config, const char *value)
{
    int val = safe_atoi(value, 1);
    if (val >= 1 && val <= 20)
    {
        config->layout_label_margin_bar = (uint8_t)val;
        record_config_change("layout", "label_margin_bar", value);
    }
}

// ============================================================================
// Layout Section Handler
// ============================================================================

static int get_layout_config(Config *config, const char *name, const char *value)
{
    // Handle bar_width separately (validation required)
    if (strcmp(name, "bar_width") == 0)
    {
        handle_layout_bar_width(config, value);
        return 1;
    }

    // Handle label margins separately (validation required)
    if (strcmp(name, "label_margin_left") == 0)
    {
        handle_layout_label_margin_left(config, value);
        return 1;
    }

    if (strcmp(name, "label_margin_bar") == 0)
    {
        handle_layout_label_margin_bar(config, value);
        return 1;
    }

    static const MixedConfigEntry entries[] = {
        {"bar_height", offsetof(Config, layout_bar_height), TYPE_UINT16, 0},
        {"bar_gap", offsetof(Config, layout_bar_gap), TYPE_UINT16, 0},
        {"bar_border", offsetof(Config, layout_bar_border), TYPE_FLOAT, 0}};

    return handle_mixed_config(config, name, value, entries, sizeof(entries) / sizeof(entries[0]));
}

// ============================================================================
// Font Section Handler
// ============================================================================

static void handle_font_size_temp(Config *config, const char *value)
{
    // Calculate dynamic default based on display size (same formula as auto-scaling)
    const double base_resolution = 240.0;
    const double base_font_size_temp = 100.0;
    const double scale_factor = ((double)config->display_width + (double)config->display_height) / (2.0 * base_resolution);
    const float dynamic_default = (float)(base_font_size_temp * scale_factor);

    float parsed = safe_atof(value, dynamic_default);
    if (parsed > 0.0f)
    {
        config->font_size_temp = parsed;
    }
}

static void handle_font_size_labels(Config *config, const char *value)
{
    // Calculate dynamic default based on display size
    const double base_resolution = 240.0;
    const double base_font_size_labels = 30.0;
    const double scale_factor = ((double)config->display_width + (double)config->display_height) / (2.0 * base_resolution);
    const float dynamic_default = (float)(base_font_size_labels * scale_factor);

    float parsed = safe_atof(value, dynamic_default);
    if (parsed > 0.0f)
    {
        config->font_size_labels = parsed;
    }
}

static int get_font_config(Config *config, const char *name, const char *value)
{
    // Handle font_face as string
    if (strcmp(name, "font_face") == 0)
    {
        if (value && value[0] != '\0')
        {
            cc_safe_strcpy(config->font_face, sizeof(config->font_face), value);
        }
        return 1;
    }

    // Handle font sizes with dynamic defaults
    if (strcmp(name, "font_size_temp") == 0)
    {
        handle_font_size_temp(config, value);
        return 1;
    }

    if (strcmp(name, "font_size_labels") == 0)
    {
        handle_font_size_labels(config, value);
        return 1;
    }

    return 1;
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

    // Handle comma-separated CPU,GPU temperature offsets
    if (strcmp(name, "display_temp_offset_x") == 0)
    {
        char *comma = strchr(value, ',');
        if (comma)
        {
            // Format: "cpu_value,gpu_value"
            char cpu_str[32] = {0};
            char gpu_str[32] = {0};
            size_t cpu_len = (size_t)(comma - value);
            // Use snprintf for safe buffer copy with size validation
            if (cpu_len > 0 && cpu_len < sizeof(cpu_str))
            {
                int written = snprintf(cpu_str, sizeof(cpu_str), "%.*s", (int)cpu_len, value);
                if (written < 0 || (size_t)written >= sizeof(cpu_str))
                    cpu_str[0] = '\0'; // Truncation safety
            }
            cc_safe_strcpy(gpu_str, sizeof(gpu_str), comma + 1);
            config->display_temp_offset_x_cpu = safe_atoi(cpu_str, -9999);
            config->display_temp_offset_x_gpu = safe_atoi(gpu_str, -9999);
            record_config_change("display_positioning", "display_temp_offset_x", value);
        }
        else
        {
            // Single value applies to both
            int val = safe_atoi(value, -9999);
            config->display_temp_offset_x_cpu = val;
            config->display_temp_offset_x_gpu = val;
            record_config_change("display_positioning", "display_temp_offset_x", value);
        }
        return 1;
    }

    if (strcmp(name, "display_temp_offset_y") == 0)
    {
        char *comma = strchr(value, ',');
        if (comma)
        {
            // Format: "cpu_value,gpu_value"
            char cpu_str[32] = {0};
            char gpu_str[32] = {0};
            size_t cpu_len = (size_t)(comma - value);
            // Use snprintf for safe buffer copy with size validation
            if (cpu_len > 0 && cpu_len < sizeof(cpu_str))
            {
                int written = snprintf(cpu_str, sizeof(cpu_str), "%.*s", (int)cpu_len, value);
                if (written < 0 || (size_t)written >= sizeof(cpu_str))
                    cpu_str[0] = '\0'; // Truncation safety
            }
            cc_safe_strcpy(gpu_str, sizeof(gpu_str), comma + 1);
            config->display_temp_offset_y_cpu = safe_atoi(cpu_str, -9999);
            config->display_temp_offset_y_gpu = safe_atoi(gpu_str, -9999);
            record_config_change("display_positioning", "display_temp_offset_y", value);
        }
        else
        {
            // Single value applies to both
            int val = safe_atoi(value, -9999);
            config->display_temp_offset_y_cpu = val;
            config->display_temp_offset_y_gpu = val;
            record_config_change("display_positioning", "display_temp_offset_y", value);
        }
        return 1;
    }

    static const PositioningConfigEntry entries[] = {
        {"display_temp_offset_x_liquid", offsetof(Config, display_temp_offset_x_liquid)},
        {"display_temp_offset_y_liquid", offsetof(Config, display_temp_offset_y_liquid)},
        {"display_degree_spacing", offsetof(Config, display_degree_spacing)},
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
        {"temp_max_scale", offsetof(Config, temp_max_scale)},
        {"liquid_max_scale", offsetof(Config, temp_liquid_max_scale)},
        {"liquid_threshold_1", offsetof(Config, temp_liquid_threshold_1)},
        {"liquid_threshold_2", offsetof(Config, temp_liquid_threshold_2)},
        {"liquid_threshold_3", offsetof(Config, temp_liquid_threshold_3)}};

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
        {"temp_threshold_4_bar", offsetof(Config, temp_threshold_4_bar)},
        {"liquid_threshold_1_bar", offsetof(Config, temp_liquid_threshold_1_bar)},
        {"liquid_threshold_2_bar", offsetof(Config, temp_liquid_threshold_2_bar)},
        {"liquid_threshold_3_bar", offsetof(Config, temp_liquid_threshold_3_bar)},
        {"liquid_threshold_4_bar", offsetof(Config, temp_liquid_threshold_4_bar)}};

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
