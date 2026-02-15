/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief JSON configuration loader with hardcoded defaults.
 * @details Parses config.json, applies defaults for missing values.
 */

#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

// cppcheck-suppress-begin missingIncludeSystem
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>
// cppcheck-suppress-end missingIncludeSystem

#include "config.h"
#include "../srv/cc_conf.h"

// ============================================================================
// Global Logging Implementation
// ============================================================================

/**
 * @brief Global logging implementation for all modules except main.c.
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

// ============================================================================
// Default Values Implementation
// ============================================================================

/**
 * @brief Set daemon default values.
 */
static void set_daemon_defaults(Config *config)
{
    if (config->daemon_address[0] == '\0')
    {
        SAFE_STRCPY(config->daemon_address, "http://localhost:11987");
    }
    if (config->daemon_password[0] == '\0')
    {
        SAFE_STRCPY(config->daemon_password, "");
    }
}

/**
 * @brief Set paths default values.
 */
static void set_paths_defaults(Config *config)
{
    if (config->paths_images[0] == '\0')
    {
        SAFE_STRCPY(config->paths_images, "/etc/coolercontrol/plugins/coolerdash");
    }
    if (config->paths_image_coolerdash[0] == '\0')
    {
        SAFE_STRCPY(config->paths_image_coolerdash, "/etc/coolercontrol/plugins/coolerdash/coolerdash.png");
    }
    if (config->paths_image_shutdown[0] == '\0')
    {
        SAFE_STRCPY(config->paths_image_shutdown,
                    "/etc/coolercontrol/plugins/coolerdash/shutdown.png");
    }
}

/**
 * @brief Try to set display dimensions from LCD device.
 */
static void try_set_lcd_dimensions(Config *config)
{
    if (config->display_width != 0 && config->display_height != 0)
        return;

    int lcd_width = 0, lcd_height = 0;
    if (!get_liquidctl_data(config, NULL, 0, NULL, 0, &lcd_width, &lcd_height))
        return;

    if (lcd_width <= 0 || lcd_height <= 0)
        return;

    if (config->display_width == 0)
        config->display_width = lcd_width;
    if (config->display_height == 0)
        config->display_height = lcd_height;
}

/**
 * @brief Set display default values with LCD device fallback.
 */
static void set_display_defaults(Config *config)
{
    try_set_lcd_dimensions(config);

    if (config->display_refresh_interval == 0.0f)
        config->display_refresh_interval = 2.50f;
    if (config->lcd_brightness == 0)
        config->lcd_brightness = 80;
    if (!is_valid_orientation(config->lcd_orientation))
        config->lcd_orientation = 0;
    if (config->display_shape[0] == '\0')
        cc_safe_strcpy(config->display_shape, sizeof(config->display_shape), "auto");
    if (config->display_mode[0] == '\0')
        cc_safe_strcpy(config->display_mode, sizeof(config->display_mode), "dual");
    if (config->circle_switch_interval == 0)
        config->circle_switch_interval = 5;
    if (config->display_content_scale_factor == 0.0f)
        config->display_content_scale_factor = 0.98f;
    if (config->display_inscribe_factor < 0.0f)
        config->display_inscribe_factor = 0.70710678f;

    // Sensor slot defaults (flexible sensor assignment)
    if (config->sensor_slot_up[0] == '\0')
        cc_safe_strcpy(config->sensor_slot_up, sizeof(config->sensor_slot_up), "cpu");
    if (config->sensor_slot_mid[0] == '\0')
        cc_safe_strcpy(config->sensor_slot_mid, sizeof(config->sensor_slot_mid), "liquid");
    if (config->sensor_slot_down[0] == '\0')
        cc_safe_strcpy(config->sensor_slot_down, sizeof(config->sensor_slot_down), "gpu");
}

/**
 * @brief Set layout default values.
 */
static void set_layout_defaults(Config *config)
{
    if (config->layout_bar_width == 0)
        config->layout_bar_width = 98;
    if (config->layout_label_margin_left == 0)
        config->layout_label_margin_left = 1;
    if (config->layout_label_margin_bar == 0)
        config->layout_label_margin_bar = 1;
    if (config->layout_bar_height == 0)
        config->layout_bar_height = 24;
    if (config->layout_bar_gap == 0)
        config->layout_bar_gap = 12.0f;
    // bar_border: -1 = use default (1.0), 0 = explicitly disabled, >0 = custom value
    if (config->layout_bar_border < 0.0f)
        config->layout_bar_border = 1.0f;
    // bar_border_enabled: -1 = auto (enabled), 0 = disabled, 1 = enabled
    if (config->layout_bar_border_enabled < 0)
        config->layout_bar_border_enabled = 1; // Default: enabled

    // Individual bar heights per slot (default to main bar_height)
    if (config->layout_bar_height_up == 0)
        config->layout_bar_height_up = config->layout_bar_height;
    if (config->layout_bar_height_mid == 0)
        config->layout_bar_height_mid = config->layout_bar_height;
    if (config->layout_bar_height_down == 0)
        config->layout_bar_height_down = config->layout_bar_height;
}

/**
 * @brief Set display positioning default values.
 */
static void set_display_positioning_defaults(Config *config)
{
    // 0 = automatic positioning (default behavior)
    // Note: All offset values default to 0, which means automatic positioning

    if (config->display_degree_spacing == 0)
        config->display_degree_spacing = 16;
}

/**
 * @brief Set font default values with dynamic scaling.
 */
static void set_font_defaults(Config *config)
{
    if (config->font_face[0] == '\0')
        SAFE_STRCPY(config->font_face, "Roboto Black");

    if (config->font_size_temp == 0.0f)
    {
        const double base_resolution = 240.0;
        const double base_font_size_temp = 100.0;
        const double scale_factor =
            ((double)config->display_width + (double)config->display_height) /
            (2.0 * base_resolution);
        config->font_size_temp = (float)(base_font_size_temp * scale_factor);

        log_message(
            LOG_INFO,
            "Font size (temp) auto-scaled: %.1f (display: %dx%d, scale: %.2f)",
            config->font_size_temp, config->display_width, config->display_height,
            scale_factor);
    }

    if (config->font_size_labels == 0.0f)
    {
        const double base_resolution = 240.0;
        const double base_font_size_labels = 30.0;
        const double scale_factor =
            ((double)config->display_width + (double)config->display_height) /
            (2.0 * base_resolution);
        config->font_size_labels = (float)(base_font_size_labels * scale_factor);

        log_message(
            LOG_INFO,
            "Font size (labels) auto-scaled: %.1f (display: %dx%d, scale: %.2f)",
            config->font_size_labels, config->display_width, config->display_height,
            scale_factor);
    }

    set_display_positioning_defaults(config);
}

/**
 * @brief Find or create a SensorConfig entry by sensor_id.
 * @return Pointer to entry, or NULL if array is full
 */
static SensorConfig *ensure_sensor_config(Config *config, const char *sensor_id)
{
    for (int i = 0; i < config->sensor_config_count; i++)
    {
        if (strcmp(config->sensor_configs[i].sensor_id, sensor_id) == 0)
            return &config->sensor_configs[i];
    }
    if (config->sensor_config_count >= MAX_SENSOR_CONFIGS)
        return NULL;
    SensorConfig *sc = &config->sensor_configs[config->sensor_config_count++];
    memset(sc, 0, sizeof(SensorConfig));
    cc_safe_strcpy(sc->sensor_id, sizeof(sc->sensor_id), sensor_id);
    return sc;
}

/**
 * @brief Initialize a SensorConfig with defaults for a given category.
 */
void init_default_sensor_config(SensorConfig *sc, const char *sensor_id,
                                int category)
{
    if (!sc || !sensor_id)
        return;
    memset(sc, 0, sizeof(SensorConfig));
    cc_safe_strcpy(sc->sensor_id, sizeof(sc->sensor_id), sensor_id);

    switch (category)
    {
    case 0: /* SENSOR_CATEGORY_TEMP */
        sc->threshold_1 = 55.0f;
        sc->threshold_2 = 65.0f;
        sc->threshold_3 = 75.0f;
        sc->max_scale = 115.0f;
        break;
    case 1: /* SENSOR_CATEGORY_RPM */
        sc->threshold_1 = 500.0f;
        sc->threshold_2 = 1000.0f;
        sc->threshold_3 = 1500.0f;
        sc->max_scale = 3000.0f;
        break;
    case 2: /* SENSOR_CATEGORY_DUTY */
        sc->threshold_1 = 25.0f;
        sc->threshold_2 = 50.0f;
        sc->threshold_3 = 75.0f;
        sc->max_scale = 100.0f;
        break;
    case 3: /* SENSOR_CATEGORY_WATTS */
        sc->threshold_1 = 50.0f;
        sc->threshold_2 = 100.0f;
        sc->threshold_3 = 200.0f;
        sc->max_scale = 500.0f;
        break;
    case 4: /* SENSOR_CATEGORY_FREQ */
        sc->threshold_1 = 1000.0f;
        sc->threshold_2 = 2000.0f;
        sc->threshold_3 = 3000.0f;
        sc->max_scale = 6000.0f;
        break;
    default:
        sc->threshold_1 = 55.0f;
        sc->threshold_2 = 65.0f;
        sc->threshold_3 = 75.0f;
        sc->max_scale = 115.0f;
        break;
    }

    sc->threshold_1_bar = (Color){0, 255, 0, 1};
    sc->threshold_2_bar = (Color){255, 140, 0, 1};
    sc->threshold_3_bar = (Color){255, 70, 0, 1};
    sc->threshold_4_bar = (Color){255, 0, 0, 1};
}

/**
 * @brief Find sensor configuration by sensor ID (public).
 */
const SensorConfig *get_sensor_config(const Config *config,
                                      const char *sensor_id)
{
    if (!config || !sensor_id)
        return NULL;
    for (int i = 0; i < config->sensor_config_count; i++)
    {
        if (strcmp(config->sensor_configs[i].sensor_id, sensor_id) == 0)
            return &config->sensor_configs[i];
    }
    return NULL;
}

/**
 * @brief Set default sensor configurations.
 * @details Ensures cpu, gpu, liquid configs exist with proper threshold defaults.
 */
static void set_default_sensor_configs(Config *config)
{
    /* Ensure CPU config */
    SensorConfig *cpu = ensure_sensor_config(config, "cpu");
    if (cpu)
    {
        if (cpu->threshold_1 == 0.0f)
            cpu->threshold_1 = 55.0f;
        if (cpu->threshold_2 == 0.0f)
            cpu->threshold_2 = 65.0f;
        if (cpu->threshold_3 == 0.0f)
            cpu->threshold_3 = 75.0f;
        if (cpu->max_scale == 0.0f)
            cpu->max_scale = 115.0f;
    }

    /* Ensure GPU config */
    SensorConfig *gpu = ensure_sensor_config(config, "gpu");
    if (gpu)
    {
        if (gpu->threshold_1 == 0.0f)
            gpu->threshold_1 = 55.0f;
        if (gpu->threshold_2 == 0.0f)
            gpu->threshold_2 = 65.0f;
        if (gpu->threshold_3 == 0.0f)
            gpu->threshold_3 = 75.0f;
        if (gpu->max_scale == 0.0f)
            gpu->max_scale = 115.0f;
    }

    /* Ensure Liquid config (lower thresholds) */
    SensorConfig *liquid = ensure_sensor_config(config, "liquid");
    if (liquid)
    {
        if (liquid->threshold_1 == 0.0f)
            liquid->threshold_1 = 25.0f;
        if (liquid->threshold_2 == 0.0f)
            liquid->threshold_2 = 28.0f;
        if (liquid->threshold_3 == 0.0f)
            liquid->threshold_3 = 31.0f;
        if (liquid->max_scale == 0.0f)
            liquid->max_scale = 50.0f;
    }
}

/**
 * @brief Check if color is unset.
 * @details Uses is_set flag - all RGB values (0,0,0 to 255,255,255) are valid.
 */
static inline int is_color_unset(const Color *color)
{
    return (color->is_set == 0);
}

/**
 * @brief Color default configuration entry.
 */
typedef struct
{
    Color *color_ptr;
    uint8_t r, g, b;
} ColorDefault;

/**
 * @brief Set color default values.
 */
static void set_color_defaults(Config *config)
{
    /* Global color defaults */
    ColorDefault color_defaults[] = {
        {&config->display_background_color, 0, 0, 0},
        {&config->layout_bar_color_background, 52, 52, 52},
        {&config->layout_bar_color_border, 192, 192, 192},
        {&config->font_color_temp, 255, 255, 255},
        {&config->font_color_label, 200, 200, 200}};

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

    /* Per-sensor color defaults (apply to all sensor configs) */
    for (int i = 0; i < config->sensor_config_count; i++)
    {
        SensorConfig *sc = &config->sensor_configs[i];
        if (is_color_unset(&sc->threshold_1_bar))
            sc->threshold_1_bar = (Color){0, 255, 0, 1};
        if (is_color_unset(&sc->threshold_2_bar))
            sc->threshold_2_bar = (Color){255, 140, 0, 1};
        if (is_color_unset(&sc->threshold_3_bar))
            sc->threshold_3_bar = (Color){255, 70, 0, 1};
        if (is_color_unset(&sc->threshold_4_bar))
            sc->threshold_4_bar = (Color){255, 0, 0, 1};
    }
}

/**
 * @brief Check if a sensor slot value is valid.
 * @details Accepts legacy ("cpu","gpu","liquid","none") and dynamic ("uid:name").
 */
static int is_valid_sensor_slot(const char *slot)
{
    if (!slot || slot[0] == '\0')
        return 0;

    /* Legacy values */
    if (strcmp(slot, "cpu") == 0 ||
        strcmp(slot, "gpu") == 0 ||
        strcmp(slot, "liquid") == 0 ||
        strcmp(slot, "none") == 0)
        return 1;

    /* Dynamic format: "uid:sensor_name" */
    const char *sep = strchr(slot, ':');
    if (sep && sep != slot && *(sep + 1) != '\0')
        return 1;

    return 0;
}

/**
 * @brief Check if a sensor slot is active (not "none").
 */
static int slot_is_active_str(const char *slot)
{
    if (!slot || slot[0] == '\0')
        return 0;
    return strcmp(slot, "none") != 0;
}

/**
 * @brief Validate sensor slot configuration.
 * @details Checks for duplicates (excluding "none"), ensures at least one active slot,
 *          and validates slot values. Resets to defaults on critical errors.
 */
static void validate_sensor_slots(Config *config)
{
    if (!config)
        return;

    int reset_needed = 0;

    // Validate slot values (must be cpu/gpu/liquid/none)
    if (!is_valid_sensor_slot(config->sensor_slot_up))
    {
        log_message(LOG_WARNING, "Invalid sensor_slot_up value, using 'cpu'");
        cc_safe_strcpy(config->sensor_slot_up, sizeof(config->sensor_slot_up), "cpu");
    }
    if (!is_valid_sensor_slot(config->sensor_slot_mid))
    {
        log_message(LOG_WARNING, "Invalid sensor_slot_mid value, using 'liquid'");
        cc_safe_strcpy(config->sensor_slot_mid, sizeof(config->sensor_slot_mid), "liquid");
    }
    if (!is_valid_sensor_slot(config->sensor_slot_down))
    {
        log_message(LOG_WARNING, "Invalid sensor_slot_down value, using 'gpu'");
        cc_safe_strcpy(config->sensor_slot_down, sizeof(config->sensor_slot_down), "gpu");
    }

    // Check for duplicates (only among active slots, "none" can appear multiple times)
    const char *slots[] = {config->sensor_slot_up, config->sensor_slot_mid, config->sensor_slot_down};
    const char *slot_names[] = {"sensor_slot_up", "sensor_slot_mid", "sensor_slot_down"};

    for (int i = 0; i < 3; i++)
    {
        if (!slot_is_active_str(slots[i]))
            continue; // Skip "none" slots

        for (int j = i + 1; j < 3; j++)
        {
            if (!slot_is_active_str(slots[j]))
                continue; // Skip "none" slots

            if (strcmp(slots[i], slots[j]) == 0)
            {
                log_message(LOG_WARNING, "Duplicate sensor in %s and %s: '%s'. Resetting to defaults.",
                            slot_names[i], slot_names[j], slots[i]);
                reset_needed = 1;
                break;
            }
        }
        if (reset_needed)
            break;
    }

    // Check that at least one slot is active
    if (!slot_is_active_str(config->sensor_slot_up) &&
        !slot_is_active_str(config->sensor_slot_mid) &&
        !slot_is_active_str(config->sensor_slot_down))
    {
        log_message(LOG_ERROR, "All sensor slots are 'none'. At least one sensor must be active. Resetting to defaults.");
        reset_needed = 1;
    }

    // Reset to defaults if validation failed
    if (reset_needed)
    {
        cc_safe_strcpy(config->sensor_slot_up, sizeof(config->sensor_slot_up), "cpu");
        cc_safe_strcpy(config->sensor_slot_mid, sizeof(config->sensor_slot_mid), "liquid");
        cc_safe_strcpy(config->sensor_slot_down, sizeof(config->sensor_slot_down), "gpu");
        log_message(LOG_STATUS, "Sensor slots reset to defaults: up=cpu, mid=liquid, down=gpu");
    }
}

/**
 * @brief Apply all system default values for missing fields.
 */
static void apply_system_defaults(Config *config)
{
    if (!config)
        return;

    set_daemon_defaults(config);
    set_paths_defaults(config);
    set_display_defaults(config);
    set_layout_defaults(config);
    set_font_defaults(config);
    set_default_sensor_configs(config);
    set_color_defaults(config);
    validate_sensor_slots(config);
}

// ============================================================================
// JSON Loading Implementation
// ============================================================================

/**
 * @brief Read color from JSON object.
 */
static int read_color_from_json(json_t *color_obj, Color *color)
{
    if (!color_obj || !json_is_object(color_obj))
        return 0;

    json_t *r = json_object_get(color_obj, "r");
    json_t *g = json_object_get(color_obj, "g");
    json_t *b = json_object_get(color_obj, "b");

    if (!r || !g || !b || !json_is_integer(r) || !json_is_integer(g) || !json_is_integer(b))
        return 0;

    int r_val = (int)json_integer_value(r);
    int g_val = (int)json_integer_value(g);
    int b_val = (int)json_integer_value(b);

    if (r_val < 0 || r_val > 255 || g_val < 0 || g_val > 255 || b_val < 0 || b_val > 255)
        return 0;

    color->r = (uint8_t)r_val;
    color->g = (uint8_t)g_val;
    color->b = (uint8_t)b_val;
    color->is_set = 1; // Mark as user-defined

    return 1;
}

/**
 * @brief Try to locate config.json.
 */
static const char *find_config_json(const char *custom_path)
{
    if (custom_path)
    {
        FILE *fp = fopen(custom_path, "r");
        if (fp)
        {
            fclose(fp);
            return custom_path;
        }
    }

    static const char *possible_paths[] = {
        "/etc/coolercontrol/plugins/coolerdash/config.json",
        NULL};

    for (int i = 0; possible_paths[i] != NULL; i++)
    {
        FILE *fp = fopen(possible_paths[i], "r");
        if (fp)
        {
            fclose(fp);
            return possible_paths[i];
        }
    }

    return NULL;
}

/**
 * @brief Load daemon settings from JSON.
 */
static void load_daemon_from_json(json_t *root, Config *config)
{
    json_t *daemon = json_object_get(root, "daemon");
    if (!daemon || !json_is_object(daemon))
        return;

    json_t *address = json_object_get(daemon, "address");
    if (address && json_is_string(address) && json_string_length(address) > 0)
    {
        const char *value = json_string_value(address);
        if (value)
        {
            SAFE_STRCPY(config->daemon_address, value);
        }
    }

    json_t *password = json_object_get(daemon, "password");
    if (password && json_is_string(password) && json_string_length(password) > 0)
    {
        const char *value = json_string_value(password);
        if (value)
        {
            SAFE_STRCPY(config->daemon_password, value);
        }
    }
}

/**
 * @brief Load paths from JSON.
 */
static void load_paths_from_json(json_t *root, Config *config)
{
    json_t *paths = json_object_get(root, "paths");
    if (!paths || !json_is_object(paths))
        return;

    json_t *images = json_object_get(paths, "images");
    if (images && json_is_string(images))
    {
        const char *value = json_string_value(images);
        if (value)
        {
            SAFE_STRCPY(config->paths_images, value);
        }
    }

    json_t *image_coolerdash = json_object_get(paths, "image_coolerdash");
    if (image_coolerdash && json_is_string(image_coolerdash))
    {
        const char *value = json_string_value(image_coolerdash);
        if (value)
        {
            SAFE_STRCPY(config->paths_image_coolerdash, value);
        }
    }

    json_t *image_shutdown = json_object_get(paths, "image_shutdown");
    if (image_shutdown && json_is_string(image_shutdown))
    {
        const char *value = json_string_value(image_shutdown);
        if (value)
        {
            SAFE_STRCPY(config->paths_image_shutdown, value);
        }
    }
}

/**
 * @brief Load display settings from JSON.
 */
static void load_display_from_json(json_t *root, Config *config)
{
    json_t *display = json_object_get(root, "display");
    if (!display || !json_is_object(display))
        return;

    json_t *mode = json_object_get(display, "mode");
    if (mode && json_is_string(mode))
    {
        const char *value = json_string_value(mode);
        if (value)
        {
            SAFE_STRCPY(config->display_mode, value);
        }
    }

    json_t *circle_interval = json_object_get(display, "circle_switch_interval");
    if (circle_interval && json_is_integer(circle_interval))
    {
        int val = (int)json_integer_value(circle_interval);
        if (val >= 1 && val <= 60)
            config->circle_switch_interval = (uint16_t)val;
    }

    json_t *refresh = json_object_get(display, "refresh_interval");
    if (refresh && json_is_number(refresh))
    {
        double val = json_number_value(refresh);
        if (val >= 0.2 && val <= 60.0)
            config->display_refresh_interval = (float)val;
    }

    json_t *brightness = json_object_get(display, "brightness");
    if (brightness && json_is_integer(brightness))
    {
        int val = (int)json_integer_value(brightness);
        if (val >= 0 && val <= 100)
            config->lcd_brightness = (uint8_t)val;
    }

    json_t *orientation = json_object_get(display, "orientation");
    if (orientation && json_is_integer(orientation))
    {
        int val = (int)json_integer_value(orientation);
        if (is_valid_orientation(val))
            config->lcd_orientation = (uint8_t)val;
    }

    json_t *width = json_object_get(display, "width");
    if (width && json_is_integer(width))
    {
        int val = (int)json_integer_value(width);
        if (val >= 100 && val <= 1024)
            config->display_width = (uint16_t)val;
    }

    json_t *height = json_object_get(display, "height");
    if (height && json_is_integer(height))
    {
        int val = (int)json_integer_value(height);
        if (val >= 100 && val <= 1024)
            config->display_height = (uint16_t)val;
    }

    json_t *shape = json_object_get(display, "shape");
    if (shape && json_is_string(shape))
    {
        SAFE_STRCPY(config->display_shape, json_string_value(shape));
    }

    json_t *scale_factor = json_object_get(display, "content_scale_factor");
    if (scale_factor && json_is_number(scale_factor))
    {
        double val = json_number_value(scale_factor);
        if (val >= 0.5 && val <= 1.0)
            config->display_content_scale_factor = (float)val;
    }

    json_t *inscribe = json_object_get(display, "inscribe_factor");
    if (inscribe && json_is_number(inscribe))
    {
        double val = json_number_value(inscribe);
        if (val >= 0.0 && val <= 1.0)
            config->display_inscribe_factor = (float)val;
    }

    // Sensor slot configuration
    json_t *slot_up = json_object_get(display, "sensor_slot_up");
    if (slot_up && json_is_string(slot_up))
    {
        const char *value = json_string_value(slot_up);
        if (value)
            cc_safe_strcpy(config->sensor_slot_up, sizeof(config->sensor_slot_up), value);
    }
    else if (slot_up && !json_is_string(slot_up))
    {
        log_message(LOG_WARNING, "sensor_slot_up has non-string type, using default '%s'",
                    config->sensor_slot_up);
    }

    json_t *slot_mid = json_object_get(display, "sensor_slot_mid");
    if (slot_mid && json_is_string(slot_mid))
    {
        const char *value = json_string_value(slot_mid);
        if (value)
            cc_safe_strcpy(config->sensor_slot_mid, sizeof(config->sensor_slot_mid), value);
    }
    else if (slot_mid && !json_is_string(slot_mid))
    {
        log_message(LOG_WARNING, "sensor_slot_mid has non-string type, using default '%s'",
                    config->sensor_slot_mid);
    }

    json_t *slot_down = json_object_get(display, "sensor_slot_down");
    if (slot_down && json_is_string(slot_down))
    {
        const char *value = json_string_value(slot_down);
        if (value)
            cc_safe_strcpy(config->sensor_slot_down, sizeof(config->sensor_slot_down), value);
    }
    else if (slot_down && !json_is_string(slot_down))
    {
        log_message(LOG_WARNING, "sensor_slot_down has non-string type, using default '%s'",
                    config->sensor_slot_down);
    }
}

/**
 * @brief Load layout settings from JSON.
 */
static void load_layout_from_json(json_t *root, Config *config)
{
    json_t *layout = json_object_get(root, "layout");
    if (!layout || !json_is_object(layout))
        return;

    json_t *bar_height = json_object_get(layout, "bar_height");
    if (bar_height && json_is_integer(bar_height))
    {
        int val = (int)json_integer_value(bar_height);
        if (val > 0 && val <= 100)
            config->layout_bar_height = (uint16_t)val;
    }

    json_t *bar_width = json_object_get(layout, "bar_width");
    if (bar_width && json_is_integer(bar_width))
    {
        int val = (int)json_integer_value(bar_width);
        if (val >= 1 && val <= 100)
            config->layout_bar_width = (uint8_t)val;
    }

    json_t *bar_gap = json_object_get(layout, "bar_gap");
    if (bar_gap && json_is_number(bar_gap))
    {
        double val = json_number_value(bar_gap);
        if (val >= 0 && val <= 100)
            config->layout_bar_gap = (uint16_t)val;
    }

    json_t *bar_border = json_object_get(layout, "bar_border");
    if (bar_border && json_is_number(bar_border))
    {
        double val = json_number_value(bar_border);
        if (val >= 0.0 && val <= 10.0)
            config->layout_bar_border = (float)val;
        // Note: -1 in JSON means "use default" (handled in set_layout_defaults)
    }

    json_t *bar_border_enabled = json_object_get(layout, "bar_border_enabled");
    if (bar_border_enabled)
    {
        if (json_is_boolean(bar_border_enabled))
            config->layout_bar_border_enabled = json_is_true(bar_border_enabled) ? 1 : 0;
        else if (json_is_integer(bar_border_enabled))
            config->layout_bar_border_enabled = (int)json_integer_value(bar_border_enabled) != 0 ? 1 : 0;
    }

    json_t *label_margin_left = json_object_get(layout, "label_margin_left");
    if (label_margin_left && json_is_integer(label_margin_left))
    {
        int val = (int)json_integer_value(label_margin_left);
        if (val >= 1 && val <= 50)
            config->layout_label_margin_left = (uint8_t)val;
    }

    json_t *label_margin_bar = json_object_get(layout, "label_margin_bar");
    if (label_margin_bar && json_is_integer(label_margin_bar))
    {
        int val = (int)json_integer_value(label_margin_bar);
        if (val >= 1 && val <= 20)
            config->layout_label_margin_bar = (uint8_t)val;
    }

    // Individual bar heights per slot
    json_t *bar_height_up = json_object_get(layout, "bar_height_up");
    if (bar_height_up && json_is_integer(bar_height_up))
    {
        int val = (int)json_integer_value(bar_height_up);
        if (val > 0 && val <= 100)
            config->layout_bar_height_up = (uint16_t)val;
    }

    json_t *bar_height_mid = json_object_get(layout, "bar_height_mid");
    if (bar_height_mid && json_is_integer(bar_height_mid))
    {
        int val = (int)json_integer_value(bar_height_mid);
        if (val > 0 && val <= 100)
            config->layout_bar_height_mid = (uint16_t)val;
    }

    json_t *bar_height_down = json_object_get(layout, "bar_height_down");
    if (bar_height_down && json_is_integer(bar_height_down))
    {
        int val = (int)json_integer_value(bar_height_down);
        if (val > 0 && val <= 100)
            config->layout_bar_height_down = (uint16_t)val;
    }
}

/**
 * @brief Load colors from JSON.
 */
static void load_colors_from_json(json_t *root, Config *config)
{
    json_t *colors = json_object_get(root, "colors");
    if (!colors || !json_is_object(colors))
        return;

    read_color_from_json(json_object_get(colors, "display_background"), &config->display_background_color);
    read_color_from_json(json_object_get(colors, "bar_background"), &config->layout_bar_color_background);
    read_color_from_json(json_object_get(colors, "bar_border"), &config->layout_bar_color_border);
    read_color_from_json(json_object_get(colors, "font_temp"), &config->font_color_temp);
    read_color_from_json(json_object_get(colors, "font_label"), &config->font_color_label);
}

/**
 * @brief Load font settings from JSON.
 */
static void load_font_from_json(json_t *root, Config *config)
{
    json_t *font = json_object_get(root, "font");
    if (!font || !json_is_object(font))
        return;

    json_t *face = json_object_get(font, "face");
    if (face && json_is_string(face))
    {
        SAFE_STRCPY(config->font_face, json_string_value(face));
    }

    json_t *size_temp = json_object_get(font, "size_temp");
    if (size_temp && json_is_number(size_temp))
    {
        double val = json_number_value(size_temp);
        if (val >= 10.0 && val <= 500.0)
            config->font_size_temp = (float)val;
    }

    json_t *size_labels = json_object_get(font, "size_labels");
    if (size_labels && json_is_number(size_labels))
    {
        double val = json_number_value(size_labels);
        if (val >= 5.0 && val <= 100.0)
            config->font_size_labels = (float)val;
    }
}

/**
 * @brief Load a single sensor config entry from a JSON object.
 * @details Generic loader for thresholds, max_scale, colors, and offsets.
 */
static void load_sensor_config_from_json(json_t *obj, SensorConfig *sc)
{
    if (!obj || !json_is_object(obj) || !sc)
        return;

    json_t *val;

    val = json_object_get(obj, "threshold_1");
    if (val && json_is_number(val))
        sc->threshold_1 = (float)json_number_value(val);

    val = json_object_get(obj, "threshold_2");
    if (val && json_is_number(val))
        sc->threshold_2 = (float)json_number_value(val);

    val = json_object_get(obj, "threshold_3");
    if (val && json_is_number(val))
        sc->threshold_3 = (float)json_number_value(val);

    val = json_object_get(obj, "max_scale");
    if (val && json_is_number(val))
        sc->max_scale = (float)json_number_value(val);

    read_color_from_json(json_object_get(obj, "threshold_1_color"), &sc->threshold_1_bar);
    read_color_from_json(json_object_get(obj, "threshold_2_color"), &sc->threshold_2_bar);
    read_color_from_json(json_object_get(obj, "threshold_3_color"), &sc->threshold_3_bar);
    read_color_from_json(json_object_get(obj, "threshold_4_color"), &sc->threshold_4_bar);

    val = json_object_get(obj, "offset_x");
    if (val && json_is_integer(val))
        sc->offset_x = (int)json_integer_value(val);

    val = json_object_get(obj, "offset_y");
    if (val && json_is_integer(val))
        sc->offset_y = (int)json_integer_value(val);

    val = json_object_get(obj, "font_size_temp");
    if (val && json_is_number(val))
        sc->font_size_temp = (float)json_number_value(val);

    val = json_object_get(obj, "label");
    if (val && json_is_string(val))
    {
        const char *label_str = json_string_value(val);
        if (label_str)
            cc_safe_strcpy(sc->label, sizeof(sc->label), label_str);
    }
}

/**
 * @brief Load sensor configurations from JSON "sensors" map.
 * @details New format: "sensors": { "cpu": {...}, "gpu": {...}, "uid:name": {...} }
 * Also handles legacy format with top-level "cpu", "gpu", "liquid" objects.
 */
static void load_sensors_from_json(json_t *root, Config *config)
{
    /* New format: "sensors" map */
    json_t *sensors = json_object_get(root, "sensors");
    if (sensors && json_is_object(sensors))
    {
        const char *key;
        json_t *value;
        json_object_foreach(sensors, key, value)
        {
            if (!json_is_object(value))
                continue;
            SensorConfig *sc = ensure_sensor_config(config, key);
            if (sc)
                load_sensor_config_from_json(value, sc);
        }
        return; /* New format takes priority */
    }

    /* Legacy backward compatibility: top-level "cpu", "gpu", "liquid" */
    json_t *cpu = json_object_get(root, "cpu");
    if (cpu && json_is_object(cpu))
    {
        SensorConfig *sc = ensure_sensor_config(config, "cpu");
        if (sc)
            load_sensor_config_from_json(cpu, sc);
    }

    json_t *gpu = json_object_get(root, "gpu");
    if (gpu && json_is_object(gpu))
    {
        SensorConfig *sc = ensure_sensor_config(config, "gpu");
        if (sc)
            load_sensor_config_from_json(gpu, sc);
    }

    json_t *liquid = json_object_get(root, "liquid");
    if (liquid && json_is_object(liquid))
    {
        SensorConfig *sc = ensure_sensor_config(config, "liquid");
        if (sc)
            load_sensor_config_from_json(liquid, sc);
    }

    /* Legacy positioning offsets → migrate to SensorConfig */
    json_t *positioning = json_object_get(root, "positioning");
    if (positioning && json_is_object(positioning))
    {
        const char *offset_keys[][3] = {
            {"temp_offset_x_cpu", "temp_offset_y_cpu", "cpu"},
            {"temp_offset_x_gpu", "temp_offset_y_gpu", "gpu"},
            {"temp_offset_x_liquid", "temp_offset_y_liquid", "liquid"}};

        for (int i = 0; i < 3; i++)
        {
            SensorConfig *sc = ensure_sensor_config(config, offset_keys[i][2]);
            if (!sc)
                continue;

            json_t *ox = json_object_get(positioning, offset_keys[i][0]);
            if (ox && json_is_integer(ox))
                sc->offset_x = (int)json_integer_value(ox);

            json_t *oy = json_object_get(positioning, offset_keys[i][1]);
            if (oy && json_is_integer(oy))
                sc->offset_y = (int)json_integer_value(oy);
        }
    }
}

/**
 * @brief Load global positioning settings from JSON.
 * @details Per-sensor offsets are now handled in load_sensors_from_json().
 */
static void load_positioning_from_json(json_t *root, Config *config)
{
    json_t *positioning = json_object_get(root, "positioning");
    if (!positioning || !json_is_object(positioning))
        return;

    json_t *degree_spacing = json_object_get(positioning, "degree_spacing");
    if (degree_spacing && json_is_integer(degree_spacing))
    {
        config->display_degree_spacing = (int)json_integer_value(degree_spacing);
    }

    json_t *label_offset_x = json_object_get(positioning, "label_offset_x");
    if (label_offset_x && json_is_integer(label_offset_x))
    {
        config->display_label_offset_x = (int)json_integer_value(label_offset_x);
    }

    json_t *label_offset_y = json_object_get(positioning, "label_offset_y");
    if (label_offset_y && json_is_integer(label_offset_y))
    {
        config->display_label_offset_y = (int)json_integer_value(label_offset_y);
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

/**
 * @brief Load complete configuration from config.json with hardcoded defaults.
 */
int load_plugin_config(Config *config, const char *config_path)
{
    if (!config)
    {
        return 0;
    }

    // Initialize with defaults (memset sets all to 0, including color.is_set = 0)
    memset(config, 0, sizeof(Config));
    config->display_inscribe_factor = -1.0f; // Sentinel for "auto"
    config->layout_bar_border = -1.0f;       // Sentinel for "use default"
    config->layout_bar_border_enabled = -1;  // Sentinel for "auto" (enabled)
    // Note: All colors have is_set=0 after memset, so defaults will be applied

    // Try to find and load JSON config
    const char *json_path = find_config_json(config_path);
    int loaded_from_json = 0;

    if (json_path)
    {
        log_message(LOG_INFO, "Loading plugin config from: %s", json_path);

        json_error_t error;
        json_t *root = json_load_file(json_path, 0, &error);

        if (root)
        {
            load_daemon_from_json(root, config);
            load_paths_from_json(root, config);
            load_display_from_json(root, config);
            load_layout_from_json(root, config);
            load_colors_from_json(root, config);
            load_font_from_json(root, config);
            load_sensors_from_json(root, config);
            load_positioning_from_json(root, config);

            json_decref(root);
            loaded_from_json = 1;
            log_message(LOG_STATUS, "Plugin configuration loaded from JSON");
        }
        else
        {
            log_message(LOG_WARNING, "Failed to parse %s: %s (line %d)",
                        json_path, error.text, error.line);
            log_message(LOG_STATUS, "Using hardcoded defaults");
        }
    }
    else
    {
        log_message(LOG_INFO, "No config.json found, using hardcoded defaults");
    }

    // Apply defaults for any missing fields
    apply_system_defaults(config);

    return loaded_from_json;
}
