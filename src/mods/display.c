/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

#include <string.h>

#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "../srv/cc_sensor.h"
#include "circle.h"
#include "display.h"
#include "custom.h"
#include "dual.h"
#include "split.h"
#include "tty.h"
#include "background.h"

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details Examines display_mode configuration and dispatches to dual, circle,
 * or split renderer. This provides a clean separation between mode selection
 * logic and mode-specific rendering.
 * @param config Configuration containing display mode and rendering parameters
 */
void reset_display_state(void)
{
    reset_background_state();
    reset_circle_state();
    reset_custom_state();
}

static int render_overlay_frame(const Config *config, const monitor_sensor_data_t *data,
                                const char *device_name)
{
    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);
    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;
    paint_display_background(cr, config);
    int success = paint_display_overlay(cr, config, data, &params) &&
                  write_display_png(surface, config, config->paths_image_coolerdash) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return success;
}

static void draw_overlay_image(const Config *config)
{
    monitor_sensor_data_t data = {0};
    if (display_needs_sensor_data(config) && !get_sensor_monitor_data(config, &data))
        data.sensor_count = 0;

    char device_uid[CC_UID_SIZE] = {0};
    char device_name[CONFIG_MAX_STRING_LEN] = {0};
    int width = 0;
    int height = 0;
    int available = get_cached_lcd_device_data(config, device_uid, sizeof(device_uid),
                                               device_name, sizeof(device_name), &width, &height);
    if (!render_overlay_frame(config, &data, device_name))
        log_message(LOG_ERROR, "Display rendering failed in %s mode", config->display_mode);
    else if (available && is_session_initialized() && device_uid[0])
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);
}

typedef struct
{
    const char *name;
    int uses_sensor_slots;
    void (*draw)(const Config *);
    int (*preview)(const Config *, const monitor_sensor_data_t *, const char *);
    int (*paint)(cairo_t *, const Config *, const monitor_sensor_data_t *,
                 const ScalingParams *);
} DisplayMode;

static const DisplayMode *get_display_mode(const Config *config)
{
    static const DisplayMode modes[] = {
        {"dual", 1, draw_dual_image, render_dual_preview, paint_dual_overlay},
        {"circle", 1, draw_circle_image, render_circle_preview, paint_circle_overlay},
        {"split", 1, draw_split_image, render_split_preview, paint_split_overlay},
        {"custom", 0, draw_overlay_image, render_overlay_frame, paint_custom_overlay},
        {"terminal", 0, draw_overlay_image, render_overlay_frame, paint_terminal_overlay},
        {"activity-rings", 0, draw_overlay_image, render_overlay_frame, paint_activity_rings_overlay},
    };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        if (strcmp(config->display_mode, modes[i].name) == 0)
            return &modes[i];
    }
    return &modes[0];
}

int display_uses_sensor_slots(const Config *config)
{
    return get_display_mode(config)->uses_sensor_slots;
}

int display_needs_sensor_data(const Config *config)
{
    if (display_uses_sensor_slots(config))
        return display_has_active_sensor_slots(config);
    return strcmp(config->display_mode, "custom") != 0 || config->custom_layout.needs_sensors;
}

int paint_display_overlay(cairo_t *cr, const struct Config *config,
                           const monitor_sensor_data_t *data,
                           const ScalingParams *params)
{
    return get_display_mode(config)->paint(cr, config, data, params);
}

void draw_display_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config parameter for draw_display_image");
        return;
    }

    if (display_uses_sensor_slots(config) && !display_has_active_sensor_slots(config))
    {
        draw_background_only_image(config);
        return;
    }

    get_display_mode(config)->draw(config);
}

static void add_preview_entry(monitor_sensor_data_t *data,
                              const char *slot_value,
                              sensor_category_t category, float value,
                              const char *unit)
{
    if (!data || !slot_is_active(slot_value) ||
        data->sensor_count >= MAX_SENSORS)
        return;

    sensor_entry_t *entry = &data->sensors[data->sensor_count++];
    const char *separator = strchr(slot_value, ':');
    if (separator && separator != slot_value)
    {
        size_t uid_length = (size_t)(separator - slot_value);
        if (uid_length >= sizeof(entry->device_uid))
            uid_length = sizeof(entry->device_uid) - 1;
        memcpy(entry->device_uid, slot_value, uid_length);
        entry->device_uid[uid_length] = '\0';
        cc_safe_strcpy(entry->name, sizeof(entry->name), separator + 1);
        cc_safe_strcpy(entry->device_name, sizeof(entry->device_name),
                       separator + 1);
        cc_safe_strcpy(entry->device_type, sizeof(entry->device_type), "Custom");
    }
    else
    {
        const char *device_type = strcmp(slot_value, "cpu") == 0
                                      ? "CPU"
                                  : strcmp(slot_value, "gpu") == 0
                                      ? "GPU"
                                      : "Liquidctl";
        cc_safe_strcpy(entry->name, sizeof(entry->name), slot_value);
        cc_safe_strcpy(entry->device_uid, sizeof(entry->device_uid), slot_value);
        char short_name[SENSOR_DEVICE_NAME_LEN];
        cc_safe_strcpy(entry->device_name, sizeof(entry->device_name),
                       get_slot_label(NULL, NULL, slot_value, short_name));
        cc_safe_strcpy(entry->device_type, sizeof(entry->device_type), device_type);
    }
    cc_safe_strcpy(entry->unit, sizeof(entry->unit), unit);
    entry->category = category;
    entry->value = value;
    entry->use_decimal = category == SENSOR_CATEGORY_TEMP &&
                         strcmp(slot_value, "liquid") == 0;
}

static void add_preview_sensor(monitor_sensor_data_t *data,
                               const char *slot_value, float temperature)
{
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_TEMP, temperature,
                      "\xC2\xB0\x43");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_DUTY, 54.0f, "%");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_RPM, 1450.0f, "RPM");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_WATTS, 125.0f, "W");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_FREQ, 4200.0f, "MHz");
}

static void add_processor_preview_channels(monitor_sensor_data_t *data)
{
    add_preview_entry(data, "cpu", SENSOR_CATEGORY_DUTY, 42, "%");
    add_preview_entry(data, "gpu", SENSOR_CATEGORY_DUTY, 66, "%");
    add_preview_entry(data, "cpu", SENSOR_CATEGORY_WATTS, 48, "W");
    add_preview_entry(data, "gpu", SENSOR_CATEGORY_WATTS, 134, "W");
}

static void add_custom_preview_sensors(monitor_sensor_data_t *data, const CustomScreen *screen)
{
    static const struct
    {
        const char *suffix;
        sensor_category_t category;
        float value;
        const char *unit;
    } samples[] = {
        {" RPM", SENSOR_CATEGORY_RPM, 1450, "RPM"},
        {" Duty", SENSOR_CATEGORY_DUTY, 54, "%"},
        {" Watts", SENSOR_CATEGORY_WATTS, 125, "W"},
        {" Freq", SENSOR_CATEGORY_FREQ, 4200, "MHz"},
    };
    int needs_processor_channels = 0;
    for (int i = 0; i < screen->count; i++)
    {
        const char *source = screen->elements[i].source;
        const CustomSource kind = get_custom_source(source);
        if (kind != CUSTOM_SOURCE_SENSOR)
        {
            if (kind != CUSTOM_SOURCE_RAM_USED && kind != CUSTOM_SOURCE_RAM_LOAD)
                needs_processor_channels = 1;
            continue;
        }
        if (!source[0] || find_sensor_for_slot(data, source))
            continue;
        size_t length = strlen(source);
        size_t sample = 0;
        while (sample < sizeof(samples) / sizeof(samples[0]))
        {
            size_t suffix = strlen(samples[sample].suffix);
            if (length > suffix && strcmp(source + length - suffix, samples[sample].suffix) == 0)
                break;
            sample++;
        }
        if (sample < sizeof(samples) / sizeof(samples[0]))
            add_preview_entry(data, source, samples[sample].category,
                              samples[sample].value, samples[sample].unit);
        else
            add_preview_entry(data, source, SENSOR_CATEGORY_TEMP,
                              strcmp(source, "liquid") == 0 ? 32.5f :
                              strcmp(source, "gpu") == 0 ? 62.0f : 45.0f, "°C");
    }
    if (needs_processor_channels)
        add_processor_preview_channels(data);
}

int render_display_preview(const struct Config *config,
                           const char *device_name)
{
    if (!config)
        return 0;

    monitor_sensor_data_t data = {0};
    if (strcmp(config->display_mode, "terminal") == 0)
    {
        add_preview_entry(&data, "cpu", SENSOR_CATEGORY_TEMP, 54, "°C");
        add_preview_entry(&data, "gpu", SENSOR_CATEGORY_TEMP, 62, "°C");
        add_preview_entry(&data, "liquid", SENSOR_CATEGORY_TEMP, 32.5f, "°C");
        add_processor_preview_channels(&data);
    }
    else if (strcmp(config->display_mode, "activity-rings") == 0)
    {
        add_preview_entry(&data, "cpu", SENSOR_CATEGORY_TEMP, 45, "°C");
        add_preview_entry(&data, "gpu", SENSOR_CATEGORY_TEMP, 62, "°C");
        add_preview_entry(&data, "liquid", SENSOR_CATEGORY_TEMP, 32.5f, "°C");
    }
    else if (strcmp(config->display_mode, "custom") == 0)
        add_custom_preview_sensors(&data, &config->custom_layout.screens[0]);
    else
    {
        add_preview_sensor(&data, config->sensor_slot_1, 45.0f);
        add_preview_sensor(&data, config->sensor_slot_2, 62.0f);
        add_preview_sensor(&data, config->sensor_slot_3, 32.5f);
        add_preview_sensor(&data, config->sensor_ring_sensor, 32.5f);
    }

    return get_display_mode(config)->preview(config, &data, device_name);
}
