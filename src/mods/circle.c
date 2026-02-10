/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Circle mode rendering (alternating CPU/GPU).
 * @details Single sensor display with automatic 2.5s switching.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "../srv/cc_sensor.h"
#include "display.h"
#include "circle.h"

/**
 * @brief Global state for sensor alternation (slot-based cycling)
 */
static int current_slot_index = 0; // 0=up, 1=mid, 2=down
static time_t last_switch_time = 0;

/**
 * @brief Get the slot value for a given slot index
 * @param config Configuration
 * @param slot_index 0=up, 1=mid, 2=down
 * @return Slot value string ("cpu", "gpu", "liquid", "none")
 */
static const char *get_slot_value_by_index(const struct Config *config, int slot_index)
{
    if (!config)
        return "none";

    switch (slot_index)
    {
    case 0:
        return config->sensor_slot_up;
    case 1:
        return config->sensor_slot_mid;
    case 2:
        return config->sensor_slot_down;
    default:
        return "none";
    }
}

/**
 * @brief Get slot name for a given slot index
 */
static const char *get_slot_name_by_index(int slot_index)
{
    switch (slot_index)
    {
    case 0:
        return "up";
    case 1:
        return "mid";
    case 2:
        return "down";
    default:
        return "up";
    }
}

/**
 * @brief Find next active slot index (wrapping around)
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
 * @brief Check if sensor should switch based on configured interval
 */
static void update_sensor_mode(const struct Config *config)
{
    time_t current_time = time(NULL);

    if (last_switch_time == 0)
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
            const char *label = get_slot_label(slot_value);
            log_message(LOG_INFO,
                        "Circle mode: switched to %s display (slot: %s, interval: %.0fs)",
                        label ? label : "unknown",
                        get_slot_name_by_index(current_slot_index),
                        interval);
        }
    }
}

/**
 * @brief Draw single sensor display based on current slot
 * @param cr Cairo context
 * @param config Configuration
 * @param params Scaling parameters
 * @param data Sensor data
 * @param slot_value Current slot sensor value ("cpu", "gpu", "liquid")
 */
static void draw_single_sensor(cairo_t *cr, const struct Config *config,
                               const ScalingParams *params,
                               const monitor_sensor_data_t *data,
                               const char *slot_value)
{
    if (!cr || !config || !params || !data || !slot_value)
        return;

    // Skip if slot is not active
    if (!slot_is_active(slot_value))
        return;

    // Get temperature and label for current slot
    const float temp_value = get_slot_temperature(data, slot_value);
    const char *label_text = get_slot_label(slot_value);
    const float max_temp = get_slot_max_scale(config, slot_value);

    const int effective_bar_width = params->safe_bar_width;

    // Get bar height for current slot (use "up" slot as reference since circle shows one at a time)
    const int bar_height = get_slot_bar_height(config, get_slot_name_by_index(current_slot_index));

    // Calculate vertical layout - BAR is centered
    const int bar_y = (config->display_height - bar_height) / 2;
    const int bar_x = (config->display_width - effective_bar_width) / 2;

    // Temperature above the bar (10% of display height above bar)
    const int temp_spacing = (int)(config->display_height * 0.10);
    const int temp_y = bar_y - temp_spacing;

    // Draw temperature value (centered horizontally INCLUDING degree symbol)
    char temp_str[16];

    // Use 1 decimal for liquid, integer for CPU/GPU
    if (strcmp(slot_value, "liquid") == 0)
        snprintf(temp_str, sizeof(temp_str), "%.1f", temp_value);
    else
        snprintf(temp_str, sizeof(temp_str), "%d", (int)temp_value);

    const Color *value_color = &config->font_color_temp;

    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    set_cairo_color(cr, value_color);

    cairo_text_extents_t temp_ext;
    cairo_text_extents(cr, temp_str, &temp_ext);

    // Calculate degree symbol width for proper centering
    cairo_set_font_size(cr, config->font_size_temp / 1.66);
    cairo_text_extents_t degree_ext;
    cairo_text_extents(cr, "°", &degree_ext);
    cairo_set_font_size(cr, config->font_size_temp);

    // Center temperature + degree symbol as a unit
    const double total_width = temp_ext.width - 4 + degree_ext.width;
    double temp_x = (config->display_width - total_width) / 2.0;
    double final_temp_y = temp_y;

    // Apply user-defined offsets based on sensor type
    if (strcmp(slot_value, "cpu") == 0)
    {
        if (config->display_temp_offset_x_cpu != -9999)
            temp_x += config->display_temp_offset_x_cpu;
        if (config->display_temp_offset_y_cpu != -9999)
            final_temp_y += config->display_temp_offset_y_cpu;
    }
    else if (strcmp(slot_value, "gpu") == 0)
    {
        if (config->display_temp_offset_x_gpu != -9999)
            temp_x += config->display_temp_offset_x_gpu;
        if (config->display_temp_offset_y_gpu != -9999)
            final_temp_y += config->display_temp_offset_y_gpu;
    }
    else if (strcmp(slot_value, "liquid") == 0)
    {
        if (config->display_temp_offset_x_liquid != -9999)
            temp_x += config->display_temp_offset_x_liquid;
        if (config->display_temp_offset_y_liquid != -9999)
            final_temp_y += config->display_temp_offset_y_liquid;
    }

    cairo_move_to(cr, temp_x, final_temp_y);
    cairo_show_text(cr, temp_str);

    // Draw degree symbol
    const int degree_spacing = (config->display_degree_spacing > 0)
                                   ? config->display_degree_spacing
                                   : 16;
    double degree_x = temp_x + temp_ext.width + degree_spacing;
    double degree_y = final_temp_y - config->font_size_temp * 0.25;

    draw_degree_symbol(cr, degree_x, degree_y, config);

    // Draw temperature bar (centered reference point)

    // Bar background
    set_cairo_color(cr, &config->layout_bar_color_background);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, effective_bar_width, bar_height,
                                params->corner_radius);
    cairo_fill(cr);

    // Bar border (only if enabled and thickness > 0)
    if (config->layout_bar_border_enabled && config->layout_bar_border > 0.0f)
    {
        set_cairo_color(cr, &config->layout_bar_color_border);
        draw_rounded_rectangle_path(cr, bar_x, bar_y, effective_bar_width, bar_height,
                                    params->corner_radius);
        cairo_set_line_width(cr, config->layout_bar_border);
        cairo_stroke(cr);
    }

    // Bar fill (temperature-based)
    const int fill_width = calculate_temp_fill_width(temp_value, effective_bar_width, max_temp);

    if (fill_width > 0)
    {
        Color bar_color = get_slot_bar_color(config, slot_value, temp_value);
        set_cairo_color(cr, &bar_color);

        cairo_save(cr);
        draw_rounded_rectangle_path(cr, bar_x, bar_y, effective_bar_width,
                                    bar_height, params->corner_radius);
        cairo_clip(cr);
        cairo_rectangle(cr, bar_x, bar_y, fill_width, bar_height);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    // Draw label (CPU, GPU, or LIQ) - centered horizontally, close to bottom
    if (label_text)
    {
        const Color *label_color = &config->font_color_label;

        cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, config->font_size_labels);
        set_cairo_color(cr, label_color);

        cairo_text_extents_t label_text_ext;
        cairo_text_extents(cr, label_text, &label_text_ext);

        // Center label horizontally
        double label_x = (config->display_width - label_text_ext.width) / 2.0;

        // Position label 2% from bottom
        double final_label_y = config->display_height - (config->display_height * 0.02);

        // Apply user-defined offsets
        if (config->display_label_offset_x != 0)
            label_x += config->display_label_offset_x;
        if (config->display_label_offset_y != 0)
            final_label_y += config->display_label_offset_y;

        cairo_move_to(cr, label_x, final_label_y);
        cairo_show_text(cr, label_text);
    }
}

/**
 * @brief Render complete circle mode display
 */
static void render_display_content(cairo_t *cr, const struct Config *config,
                                   const monitor_sensor_data_t *data,
                                   const ScalingParams *params)
{
    if (!cr || !config || !data || !params)
        return;

    // Draw main background
    set_cairo_color(cr, &config->display_background_color);
    cairo_paint(cr);

    // Update sensor mode (check if configured interval elapsed)
    update_sensor_mode(config);

    // Get current slot value and draw sensor
    const char *slot_value = get_slot_value_by_index(config, current_slot_index);
    draw_single_sensor(cr, config, params, data, slot_value);
}

/**
 * @brief Render circle mode display to PNG file.
 * @details Creates PNG image with single sensor, does NOT upload.
 */
static int render_circle_display(const struct Config *config,
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
        const char *slot_value = get_slot_value_by_index(config, current_slot_index);
        const char *label = get_slot_label(slot_value);
        float temp = get_slot_temperature(data, slot_value);
        log_message(LOG_INFO, "Circle mode: rendering %s (%.1f°C)",
                    label ? label : "unknown", temp);
    }

    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;

    render_display_content(cr, config, data, &params);

    cairo_surface_flush(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Cairo drawing error: %s",
                    cairo_status_to_string(cairo_status(cr)));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    // Write PNG to file
    cairo_status_t write_status =
        cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    if (write_status != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to write PNG: %s",
                    cairo_status_to_string(write_status));
        return 0;
    }

    return 1;
}

/**
 * @brief High-level entry point for circle mode rendering
 * @details Collects sensor data, renders circle display using
 * render_circle_display(), and sends to LCD device.
 */
void draw_circle_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config for circle mode");
        return;
    }

    // Get device information
    char device_uid[128] = {0};
    char device_name[128] = {0};
    int screen_width = 0, screen_height = 0;

    const int device_available =
        get_liquidctl_data(config, device_uid, sizeof(device_uid), device_name,
                           sizeof(device_name), &screen_width, &screen_height);

    // Get temperature data
    monitor_sensor_data_t data = {0};
    if (!get_temperature_monitor_data(config, &data))
    {
        log_message(LOG_WARNING, "Circle mode: Failed to get temperature data");
        return;
    }

    // Render circle display with device name for circular display detection
    if (!render_circle_display(config, &data, device_name))
    {
        log_message(LOG_ERROR, "Circle display rendering failed");
        return;
    }

    // Send to LCD if available
    if (is_session_initialized() && device_available && device_uid[0] != '\0')
    {
        const char *name =
            (device_name[0] != '\0') ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending circle image to LCD: %s [%s]", name,
                    device_uid);

        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);

        log_message(LOG_INFO, "Circle LCD image uploaded successfully");
    }
    else
    {
        log_message(LOG_WARNING, "Skipping circle LCD upload - device not available");
    }
}
