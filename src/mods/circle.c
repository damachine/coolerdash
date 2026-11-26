/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Circle mode rendering (slot-based alternating display).
 * @details Single-sensor display that cycles through configured active slots.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <time.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_sensor.h"
#include "display.h"
#include "circle.h"
#include "ring.h"
#include "circle_render.h"
#include "render.h"

/**
 * @brief Global state for sensor alternation (slot-based cycling).
 */
static int current_slot_index = 0; // 0=slot1, 1=slot2, 2=slot3
static time_t last_switch_time = 0;

/**
 * @brief Resets circle mode state for config reload (SIGHUP).
 * @details Resets slot cycling to the first sensor and clears the switch timer.
 */
void reset_circle_state(void)
{
    current_slot_index = 0;
    last_switch_time = 0;
}

/**
 * @brief Get the slot value for a given slot index.
 * @param config Configuration
 * @param slot_index 0=slot1, 1=slot2, 2=slot3
 * @return Slot value string ("cpu", "gpu", "liquid", "none")
 */
static const char *get_slot_value_by_index(const struct Config *config, int slot_index)
{
    if (!config)
        return "none";

    switch (slot_index)
    {
    case 0:
        return config->sensor_slot_1;
    case 1:
        return config->sensor_slot_2;
    case 2:
        return config->sensor_slot_3;
    default:
        return "none";
    }
}

/**
 * @brief Get slot name for a given slot index.
 */
static const char *get_slot_name_by_index(int slot_index)
{
    switch (slot_index)
    {
    case 0:
        return "1";
    case 1:
        return "2";
    case 2:
        return "3";
    default:
        return "1";
    }
}

/**
 * @brief Find next active slot index (wrapping around).
 * @param config Configuration
 * @param start_index Starting slot index
 * @return Next active slot index, or -1 if none found
 */
static int find_next_active_slot(const struct Config *config, int start_index)
{
    for (int i = 0; i < 3; i++)
    {
        int idx = (start_index + i) % 3;
        const char *slot_value = get_slot_value_by_index(config, idx);
        if (slot_is_active(slot_value))
            return idx;
    }
    return -1; // No active slots
}

/**
 * @brief Check if sensor should switch based on configured interval.
 */
static void update_sensor_mode(const struct Config *config)
{
    time_t current_time = time(NULL);

    if (last_switch_time == 0 ||
        !slot_is_active(get_slot_value_by_index(config, current_slot_index)))
    {
        // Initialize to first active slot
        current_slot_index = find_next_active_slot(config, 0);
        if (current_slot_index < 0)
            current_slot_index = 0; // Fallback
        last_switch_time = current_time;
        return;
    }

    // Check if configured interval has elapsed
    const double interval = (config && config->circle_switch_interval > 0)
                                ? (double)config->circle_switch_interval
                                : 5.0; // Fallback: 5 seconds

    if (difftime(current_time, last_switch_time) >= interval)
    {
        // Find next active slot
        int next_slot = find_next_active_slot(config, (current_slot_index + 1) % 3);
        if (next_slot >= 0)
            current_slot_index = next_slot;
        last_switch_time = current_time;

        // Verbose logging only
        if (verbose_logging)
        {
            const char *slot_value = get_slot_value_by_index(config, current_slot_index);
            char short_name[SENSOR_DEVICE_NAME_LEN];
            const char *label = get_slot_label(config, NULL, slot_value, short_name);
            log_message(LOG_INFO,
                        "Circle mode: switched to %s display (slot: %s, interval: %.0fs)",
                        label ? label : "unknown",
                        get_slot_name_by_index(current_slot_index),
                        interval);
        }
    }
}

static int displayed_circle_slot(const struct Config *config)
{
    if (config->preview_render)
        return find_next_active_slot(config, 0);
    return current_slot_index;
}

static const char *displayed_circle_source(const Config *config)
{
    return get_slot_value_by_index(config, displayed_circle_slot(config));
}

int paint_circle_overlay(cairo_t *cr, const struct Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params)
{
    if (!config->preview_render)
        update_sensor_mode(config);

    const int slot = displayed_circle_slot(config);
    if (config->display_rotation == 0 || params->is_circular)
        draw_sensor_ring_for_slot(cr, config, data, params,
                                  get_slot_value_by_index(config, slot));
    paint_circle_sensor(cr, config, params, data, get_slot_value_by_index(config, slot),
                        get_slot_name_by_index(slot));
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

/**
 * @brief Render circle mode display to PNG file.
 * @details Creates PNG image with single sensor, does NOT upload.
 */
int render_circle_preview(const struct Config *config,
                          const monitor_sensor_data_t *data,
                          const char *device_name)
{
    if (!config || !data)
    {
        log_message(LOG_ERROR, "Invalid parameters for circle mode rendering");
        return 0;
    }

    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);

    // Verbose logging only
    if (verbose_logging)
    {
        const char *slot_value = displayed_circle_source(config);
        char short_name[SENSOR_DEVICE_NAME_LEN];
        const char *label = get_slot_label(config, data, slot_value, short_name);
        float temp = get_slot_temperature(data, slot_value);
        log_message(LOG_INFO,
                    "Circle mode: slot=%s label=%s temp=%.1f extra_info=%s font=%s",
                    slot_value ? slot_value : "none",
                    label ? label : "unknown", temp,
                    (config->circle_show_frequency || config->circle_show_watts ||
                     config->circle_show_rpm) ? "on" : "off",
                    config->font_face[0] ? config->font_face : "default");
    }

    const FrameRing ring = {.current_slot = displayed_circle_source};
    return render_frame(config, data, &params, paint_circle_overlay, &ring);
}
