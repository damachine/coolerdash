/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

#include <string.h>

#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "background.h"
#include "circle.h"
#include "custom.h"
#include "display.h"
#include "dual.h"
#include "render.h"
#include "rings.h"
#include "split.h"
#include "tty.h"

void reset_display_state(void)
{
    reset_background_state();
    reset_circle_state();
    reset_layout_screen_state();
}

static int render_overlay_frame(const Config *config, const monitor_sensor_data_t *data,
                                const char *device_name)
{
    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);
    return render_frame(config, data, &params, paint_display_overlay, NULL);
}

typedef struct
{
    const char *name;
    int uses_sensor_slots;
    int (*render)(const Config *, const monitor_sensor_data_t *, const char *);
    int (*paint)(cairo_t *, const Config *, const monitor_sensor_data_t *,
                 const ScalingParams *);
} DisplayMode;

static const DisplayMode *get_display_mode(const Config *config)
{
    static const DisplayMode modes[] = {
        {"dual", 1, render_dual_preview, paint_dual_overlay},
        {"circle", 1, render_circle_preview, paint_circle_overlay},
        {"split", 1, render_split_preview, paint_split_overlay},
        {"custom", 0, render_overlay_frame, paint_custom_overlay},
        {"terminal", 0, render_overlay_frame, paint_terminal_overlay},
        {"circle-rings", 0, render_overlay_frame, paint_circle_rings_overlay},
    };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        if (strcmp(config->display_mode, modes[i].name) == 0)
            return &modes[i];
    }
    return &modes[0];
}

void log_layout_box(cairo_t *cr, const struct Config *config, const char *element,
                    double x, double y, double width, double height)
{
    if (!verbose_logging || !config || width <= 0.0 || height <= 0.0)
        return;

    if (cr)
    {
        cairo_user_to_device(cr, &x, &y);
        cairo_user_to_device_distance(cr, &width, &height);
    }
    log_message(LOG_INFO, "Layout: mode=%s output=%s element=%s x=%.1f y=%.1f width=%.1f height=%.1f",
                get_display_mode(config)->name, config->preview_render ? "preview" : "lcd",
                element, x, y, width, height);
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

    const DisplayMode *mode = get_display_mode(config);
    if (mode->uses_sensor_slots && !display_has_active_sensor_slots(config))
    {
        draw_background_only_image(config);
        return;
    }

    monitor_sensor_data_t data = {0};
    if (display_needs_sensor_data(config) && !get_sensor_monitor_data(config, &data))
    {
        if (mode->uses_sensor_slots)
        {
            log_message(LOG_WARNING, "Failed to retrieve sensor data for %s", mode->name);
            return;
        }
        data.sensor_count = 0;
    }

    char uid[CC_UID_SIZE] = {0};
    char name[CONFIG_MAX_STRING_LEN] = {0};
    const int available = get_cached_lcd_device_data(config, uid, sizeof(uid), name,
                                                     sizeof(name), NULL, NULL);
    if (!mode->render(config, &data, name))
    {
        log_message(LOG_ERROR, "Display rendering failed in %s mode", mode->name);
        return;
    }

    if (available && is_session_initialized() && uid[0])
    {
        log_message(LOG_INFO, "Sending %s image to LCD: %s [%s]", mode->name, name, uid);
        send_image_to_lcd(config, config->paths_image_coolerdash, uid);
    }
    else if (mode->uses_sensor_slots)
        log_message(LOG_WARNING, "Skipping %s LCD upload because the device is unavailable", mode->name);
}

static sensor_entry_t *add_preview_entry(monitor_sensor_data_t *data,
                                         const char *slot_value,
                                         sensor_category_t category, float value,
                                         const char *unit)
{
    if (!data || !slot_is_active(slot_value) ||
        data->sensor_count >= MAX_SENSORS)
        return NULL;

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
    return entry;
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

static void add_preview_pump_duty(monitor_sensor_data_t *data)
{
    sensor_entry_t *entry = add_preview_entry(data, "liquid", SENSOR_CATEGORY_DUTY, 54, "%");
    if (entry)
        cc_safe_strcpy(entry->name, sizeof(entry->name), "pump Duty");
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
        const CustomElement *element = &screen->elements[i];
        const char *source = element->source;
        if (!source[0])
            continue;
        if (element->channel != SENSOR_CATEGORY_TEMP)
        {
            if (!find_channel_sensor_for_slot(data, source, element->channel))
                for (size_t s = 0; s < sizeof(samples) / sizeof(samples[0]); s++)
                    if (samples[s].category == element->channel)
                    {
                        sensor_entry_t *entry = add_preview_entry(
                            data, source, samples[s].category, samples[s].value, samples[s].unit);
                        /* Look up this sample by device and category, without masking a named sensor. */
                        if (entry && strchr(source, ':'))
                            entry->name[0] = '\0';
                        break;
                    }
            continue;
        }
        const CustomSource kind = get_custom_source(source);
        if (kind == CUSTOM_SOURCE_LIQUID_PUMP_DUTY)
        {
            if (!find_liquid_pump_duty(data))
                add_preview_pump_duty(data);
            continue;
        }
        if (kind != CUSTOM_SOURCE_SENSOR)
        {
            if (kind != CUSTOM_SOURCE_RAM_USED && kind != CUSTOM_SOURCE_RAM_LOAD)
                needs_processor_channels = 1;
            continue;
        }
        if (find_sensor_for_slot(data, source))
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
    else if (strcmp(config->display_mode, "circle-rings") == 0)
    {
        add_preview_entry(&data, "liquid", SENSOR_CATEGORY_TEMP, 32.5f, "°C");
        add_preview_pump_duty(&data);
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

    return get_display_mode(config)->render(config, &data, device_name);
}
