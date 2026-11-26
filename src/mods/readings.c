/* SPDX-License-Identifier: MIT */
#include <string.h>

#include "../srv/cc_conf.h"
#include "display.h"

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
