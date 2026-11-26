/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../srv/cc_conf.h"
#include "readings.h"

int reading_is_valid(const sensor_entry_t *sensor)
{
    return sensor && isfinite(sensor->value);
}

int read_processor_power(const monitor_sensor_data_t *data, double *watts, const char **label)
{
    const sensor_entry_t *cpu = find_channel_sensor_for_slot(data, "cpu", SENSOR_CATEGORY_WATTS);
    const sensor_entry_t *gpu = find_channel_sensor_for_slot(data, "gpu", SENSOR_CATEGORY_WATTS);
    const int has_cpu = reading_is_valid(cpu);
    const int has_gpu = reading_is_valid(gpu);
    if (label)
        *label = has_cpu == has_gpu ? "PWR" : has_cpu ? "CPU W" : "GPU W";
    *watts = (has_cpu ? (double)cpu->value : 0) + (has_gpu ? (double)gpu->value : 0);
    return has_cpu || has_gpu;
}

int read_used_memory(const Config *config, double *gib, double *progress)
{
    if (config->preview_render)
    {
        *gib = 18.6;
        *progress = 0.58;
        return 1;
    }
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file)
        return 0;

    unsigned long long total = 0;
    unsigned long long available = 0;
    int has_available = 0;
    char line[128];
    while ((!total || !has_available) && fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "MemAvailable: %llu kB", &available) == 1)
            has_available = 1;
        else
            sscanf(line, "MemTotal: %llu kB", &total);
    }
    fclose(file);
    if (!total || !has_available || available > total)
        return 0;

    const unsigned long long used = total - available;
    *gib = used / 1048576.0;
    *progress = (double)used / total;
    return 1;
}

static const sensor_entry_t *widget_sensor(const Config *config, const monitor_sensor_data_t *data,
                                           const char *id, WidgetReadings *readings)
{
    const CustomSource source = get_custom_source(id);
    if (source == CUSTOM_SOURCE_SENSOR)
        return find_sensor_for_slot(data, id);
    if (source == CUSTOM_SOURCE_CPU_LOAD || source == CUSTOM_SOURCE_GPU_LOAD)
        return find_processor_load(data, source == CUSTOM_SOURCE_CPU_LOAD ? "cpu" : "gpu");
    if (source == CUSTOM_SOURCE_CPU_POWER || source == CUSTOM_SOURCE_GPU_POWER)
        return find_channel_sensor_for_slot(data, source == CUSTOM_SOURCE_CPU_POWER ? "cpu" : "gpu",
                                            SENSOR_CATEGORY_WATTS);
    if (source == CUSTOM_SOURCE_LIQUID_PUMP_DUTY)
        return find_liquid_pump_duty(data);

    double value;
    const char *unit;
    if (source == CUSTOM_SOURCE_POWER)
    {
        if (!read_processor_power(data, &value, NULL))
            return NULL;
        unit = "W";
    }
    else
    {
        if (!readings->memory_status)
            readings->memory_status =
                read_used_memory(config, &readings->gib, &readings->memory_load) ? 1 : -1;
        if (readings->memory_status < 0)
            return NULL;
        value = source == CUSTOM_SOURCE_RAM_USED ? readings->gib : readings->memory_load * 100;
        unit = source == CUSTOM_SOURCE_RAM_USED ? "GiB" : "%";
    }
    readings->sensor.value = (float)value;
    readings->sensor.use_decimal = source == CUSTOM_SOURCE_RAM_USED;
    snprintf(readings->sensor.unit, sizeof(readings->sensor.unit), "%s", unit);
    return &readings->sensor;
}

const sensor_entry_t *get_widget_reading(const Config *config, const monitor_sensor_data_t *data,
                                         const CustomElement *element, WidgetReadings *readings)
{
    if (!element->source[0])
        return NULL;

    const sensor_entry_t *sensor = element->channel == SENSOR_CATEGORY_TEMP
        ? widget_sensor(config, data, element->source, readings)
        : find_channel_sensor_for_slot(data, element->source, element->channel);
    return reading_is_valid(sensor) ? sensor : NULL;
}

double widget_progress(const CustomElement *element, double value)
{
    return fmax(0.0, fmin(1.0, (value - element->minimum) /
                               (element->maximum - element->minimum)));
}

/**
 * @brief Check if a sensor slot is active.
 */
int slot_is_active(const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0')
        return 0;
    return strcmp(slot_value, "none") != 0;
}

int display_has_active_sensor_slots(const struct Config *config)
{
    return config &&
           (slot_is_active(config->sensor_slot_1) ||
            slot_is_active(config->sensor_slot_2) ||
            slot_is_active(config->sensor_slot_3));
}

/**
 * @brief Get sensor value for a slot.
 */
float get_slot_temperature(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 0.0f;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->value;

    return 0.0f;
}

/**
 * @brief Get display label for a sensor slot.
 * @details Returns custom label from SensorConfig if set, otherwise
 * uses legacy names or sensor name from data.
 */
const char *get_slot_label(const struct Config *config,
                           const monitor_sensor_data_t *data,
                           const char *slot_value,
                           char short_name[static SENSOR_DEVICE_NAME_LEN])
{
    if (!slot_value || slot_value[0] == '\0' || strcmp(slot_value, "none") == 0)
        return NULL;

    /* Check for custom label override in SensorConfig */
    if (config)
    {
        const SensorConfig *sc = get_sensor_config(config, slot_value);
        if (sc && sc->label[0] != '\0')
            return sc->label;
    }

    /* Use device name from API if available (first two words only) */
    if (data)
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
        if (entry && entry->device_name[0] != '\0')
        {
            cc_safe_strcpy(short_name, SENSOR_DEVICE_NAME_LEN,
                           entry->device_name);
            /* Find end of second word */
            int words = 0;
            for (size_t i = 0; short_name[i] != '\0'; i++)
            {
                if (short_name[i] == ' ')
                {
                    words++;
                    if (words >= 2)
                    {
                        short_name[i] = '\0';
                        break;
                    }
                }
            }
            return short_name;
        }
    }

    /* Fallback: legacy labels */
    if (strcmp(slot_value, "cpu") == 0)
        return "CPU";
    if (strcmp(slot_value, "gpu") == 0)
        return "GPU";
    if (strcmp(slot_value, "liquid") == 0)
        return "LIQ";

    /* Dynamic: use sensor name */
    if (data)
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
        if (entry)
            return entry->name;
    }

    return "???";
}

/**
 * @brief Get bar color for a sensor slot based on value.
 * @details Uses SensorConfig thresholds via get_sensor_config().
 */
Color get_slot_bar_color(const struct Config *config, const char *slot_value,
                         float value)
{
    Color default_color = {0, 255, 0, 1};

    if (!config || !slot_value)
        return default_color;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (!sc)
        return default_color;

    if (value < sc->threshold_1)
        return sc->threshold_1_bar;
    else if (value < sc->threshold_2)
        return sc->threshold_2_bar;
    else if (value < sc->threshold_3)
        return sc->threshold_3_bar;
    else
        return sc->threshold_4_bar;
}

/**
 * @brief Get maximum scale for a sensor slot.
 */
float get_slot_max_scale(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 115.0f;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (sc && sc->max_scale > 0.0f)
        return sc->max_scale;

    return 115.0f;
}

/**
 * @brief Get display unit string for a sensor slot.
 */
const char *get_slot_unit(const monitor_sensor_data_t *data,
                          const char *slot_value)
{
    if (!data || !slot_value)
        return "\xC2\xB0"
               "C";

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->unit;

    return "\xC2\xB0"
           "C";
}

/**
 * @brief Check if sensor should display decimal values.
 */
int get_slot_use_decimal(const monitor_sensor_data_t *data,
                         const char *slot_value)
{
    if (!data || !slot_value)
        return 0;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->use_decimal;

    return 0;
}

/**
 * @brief Get display X offset for a sensor slot.
 */
int get_slot_offset_x(const struct Config *config, const char *slot_value)
{
    if (!config || !slot_value)
        return 0;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    return sc ? sc->offset_x : 0;
}

/**
 * @brief Get display Y offset for a sensor slot.
 */
int get_slot_offset_y(const struct Config *config, const char *slot_value)
{
    if (!config || !slot_value)
        return 0;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    return sc ? sc->offset_y : 0;
}

/**
 * @brief Check if a sensor slot is a temperature sensor.
 */
int get_slot_is_temp(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 1; /* Default to temp */

    /* Legacy slots are always temperature */
    if (strcmp(slot_value, "cpu") == 0 || strcmp(slot_value, "gpu") == 0 ||
        strcmp(slot_value, "liquid") == 0)
        return 1;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return (entry->category == SENSOR_CATEGORY_TEMP) ? 1 : 0;

    return 1;
}

/**
 * @brief Get font size for a sensor slot.
 * @details Returns per-sensor font size if configured (> 0), otherwise global.
 */
float get_slot_font_size(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 100.0f;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (sc && sc->font_size_temp > 0.0f)
        return sc->font_size_temp;

    return config->font_size_temp;
}
