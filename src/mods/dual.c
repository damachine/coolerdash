/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Dual mode rendering (CPU+GPU side by side).
 * @details Cairo-based LCD image generation for dual sensor display.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "../srv/cc_sensor.h"
#include "display.h"
#include "dual.h"

/**
 * @brief Forward declarations for internal display rendering functions
 */
static void draw_temperature_displays(cairo_t *cr,
                                      const monitor_sensor_data_t *data,
                                      const struct Config *config,
                                      const ScalingParams *params);
static void draw_temperature_bars(cairo_t *cr,
                                  const monitor_sensor_data_t *data,
                                  const struct Config *config,
                                  const ScalingParams *params);
static void draw_single_temperature_bar_slot(cairo_t *cr,
                                             const struct Config *config,
                                             const ScalingParams *params,
                                             const char *slot_value,
                                             float temp_value, int bar_x, int bar_y,
                                             int bar_width, int bar_height);
static void draw_labels(cairo_t *cr, const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params);
static void render_display_content(cairo_t *cr, const struct Config *config,
                                   const monitor_sensor_data_t *data,
                                   const ScalingParams *params);

/**
 * @brief Draw temperature displays for up and down slots
 */
static void draw_temperature_displays(cairo_t *cr,
                                      const monitor_sensor_data_t *data,
                                      const struct Config *config,
                                      const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    const int effective_bar_width = params->safe_bar_width;
    const int bar_x = (config->display_width - effective_bar_width) / 2;

    // Get slot configurations
    const char *slot_up = config->sensor_slot_up;
    const char *slot_down = config->sensor_slot_down;
    const int up_active = slot_is_active(slot_up);
    const int down_active = slot_is_active(slot_down);

    // Get temperatures from configured slots
    float temp_up = get_slot_temperature(data, slot_up);
    float temp_down = get_slot_temperature(data, slot_down);

    // Get bar heights
    const uint16_t bar_height_up = get_slot_bar_height(config, "up");
    const uint16_t bar_height_down = get_slot_bar_height(config, "down");

    // Calculate bar positions based on active slots
    int total_height = 0;
    if (up_active && down_active)
        total_height = bar_height_up + config->layout_bar_gap + bar_height_down;
    else if (up_active)
        total_height = bar_height_up;
    else if (down_active)
        total_height = bar_height_down;
    else
        return; // No active slots

    const int start_y = (config->display_height - total_height) / 2;
    int up_bar_y = start_y;
    int down_bar_y = start_y + bar_height_up + config->layout_bar_gap;

    // If only down slot is active, center it
    if (!up_active && down_active)
        down_bar_y = start_y;

    cairo_font_extents_t font_ext;
    cairo_font_extents(cr, &font_ext);

    // Calculate reference width (widest 2-digit number) for sub-100 alignment
    cairo_text_extents_t ref_width_ext;
    cairo_text_extents(cr, "88", &ref_width_ext);

    // Draw upper slot temperature
    if (up_active)
    {
        char up_num_str[16];
        snprintf(up_num_str, sizeof(up_num_str), "%d", (int)temp_up);

        cairo_text_extents_t up_num_ext;
        cairo_text_extents(cr, up_num_str, &up_num_ext);

        double up_width = (temp_up >= 100.0f) ? up_num_ext.width : ref_width_ext.width;
        double up_temp_x = bar_x + (effective_bar_width - up_width) / 2.0;

        if (config->display_width == 240 && config->display_height == 240)
            up_temp_x += 20;

        if (config->display_temp_offset_x_cpu != 0)
            up_temp_x += config->display_temp_offset_x_cpu;

        double up_temp_y = up_bar_y + 8 - font_ext.descent;
        if (config->display_temp_offset_y_cpu != 0)
            up_temp_y += config->display_temp_offset_y_cpu;

        cairo_move_to(cr, up_temp_x, up_temp_y);
        cairo_show_text(cr, up_num_str);

        // Degree symbol
        const int degree_spacing = (config->display_degree_spacing > 0) ? config->display_degree_spacing : 16;
        double degree_up_x = up_temp_x + up_width + degree_spacing;
        double degree_up_y = up_temp_y - up_num_ext.height * 0.40;
        draw_degree_symbol(cr, degree_up_x, degree_up_y, config);
    }

    // Draw lower slot temperature
    if (down_active)
    {
        char down_num_str[16];
        snprintf(down_num_str, sizeof(down_num_str), "%d", (int)temp_down);

        cairo_text_extents_t down_num_ext;
        cairo_text_extents(cr, down_num_str, &down_num_ext);

        double down_width = (temp_down >= 100.0f) ? down_num_ext.width : ref_width_ext.width;
        double down_temp_x = bar_x + (effective_bar_width - down_width) / 2.0;

        if (config->display_width == 240 && config->display_height == 240)
            down_temp_x += 20;

        if (config->display_temp_offset_x_gpu != 0)
            down_temp_x += config->display_temp_offset_x_gpu;

        // Use the actual bar height for positioning
        uint16_t effective_down_height = down_active ? bar_height_down : 0;
        double down_temp_y = down_bar_y + effective_down_height - 4 + font_ext.ascent;
        if (config->display_temp_offset_y_gpu != 0)
            down_temp_y += config->display_temp_offset_y_gpu;

        cairo_move_to(cr, down_temp_x, down_temp_y);
        cairo_show_text(cr, down_num_str);

        // Degree symbol
        const int degree_spacing = (config->display_degree_spacing > 0) ? config->display_degree_spacing : 16;
        double degree_down_x = down_temp_x + down_width + degree_spacing;
        double degree_down_y = down_temp_y - down_num_ext.height * 0.40;
        draw_degree_symbol(cr, degree_down_x, degree_down_y, config);
    }
}

/**
 * @brief Draw a single temperature bar with background, fill, and border
 */
static void draw_single_temperature_bar_slot(cairo_t *cr,
                                             const struct Config *config,
                                             const ScalingParams *params,
                                             const char *slot_value,
                                             float temp_value, int bar_x, int bar_y,
                                             int bar_width, int bar_height)
{
    if (!cr || !config || !params)
        return;

    // Use slot-specific max scale and color
    const float max_temp = get_slot_max_scale(config, slot_value);
    const int fill_width =
        calculate_temp_fill_width(temp_value, bar_width, max_temp);

    // Background
    set_cairo_color(cr, &config->layout_bar_color_background);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, bar_width,
                                bar_height, params->corner_radius);
    cairo_fill(cr);

    // Fill
    if (fill_width > 0)
    {
        Color fill_color = get_slot_bar_color(config, slot_value, temp_value);
        set_cairo_color(cr, &fill_color);

        if (fill_width >= 16)
            draw_rounded_rectangle_path(cr, bar_x, bar_y, fill_width,
                                        bar_height,
                                        params->corner_radius);
        else
            cairo_rectangle(cr, bar_x, bar_y, fill_width, bar_height);

        cairo_fill(cr);
    }

    // Border (only if enabled and thickness > 0)
    if (config->layout_bar_border_enabled && config->layout_bar_border > 0.0f)
    {
        cairo_set_line_width(cr, config->layout_bar_border);
        set_cairo_color(cr, &config->layout_bar_color_border);
        draw_rounded_rectangle_path(cr, bar_x, bar_y, bar_width,
                                    bar_height, params->corner_radius);
        cairo_stroke(cr);
    }
}

/**
 * @brief Draw temperature bars for up and down slots
 */
static void draw_temperature_bars(cairo_t *cr,
                                  const monitor_sensor_data_t *data,
                                  const struct Config *config,
                                  const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    const double bar_side_margin = config->display_width * 0.0025;
    const int effective_bar_width =
        params->safe_bar_width - (int)(2 * bar_side_margin);
    const int bar_x = (config->display_width - effective_bar_width) / 2;

    // Get slot configurations
    const char *slot_up = config->sensor_slot_up;
    const char *slot_down = config->sensor_slot_down;
    const int up_active = slot_is_active(slot_up);
    const int down_active = slot_is_active(slot_down);

    // Get bar heights
    const uint16_t bar_height_up = get_slot_bar_height(config, "up");
    const uint16_t bar_height_down = get_slot_bar_height(config, "down");

    // Calculate positions based on active slots
    int total_height = 0;
    if (up_active && down_active)
        total_height = bar_height_up + config->layout_bar_gap + bar_height_down;
    else if (up_active)
        total_height = bar_height_up;
    else if (down_active)
        total_height = bar_height_down;
    else
        return;

    const int start_y = (config->display_height - total_height) / 2;
    int up_bar_y = start_y;
    int down_bar_y = start_y + bar_height_up + config->layout_bar_gap;

    // If only down slot is active, center it
    if (!up_active && down_active)
        down_bar_y = start_y;

    // Draw upper slot bar
    if (up_active)
    {
        float temp_up = get_slot_temperature(data, slot_up);
        draw_single_temperature_bar_slot(cr, config, params, slot_up, temp_up,
                                         bar_x, up_bar_y, effective_bar_width, bar_height_up);
    }

    // Draw lower slot bar
    if (down_active)
    {
        float temp_down = get_slot_temperature(data, slot_down);
        draw_single_temperature_bar_slot(cr, config, params, slot_down, temp_down,
                                         bar_x, down_bar_y, effective_bar_width, bar_height_down);
    }
}

/**
 * @brief Draw labels for up and down slots
 */
static void draw_labels(cairo_t *cr, const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params)
{
    (void)data; // Reserved for future use (e.g., dynamic labels based on values)

    if (!cr || !config || !params)
        return;

    // Get slot configurations
    const char *slot_up = config->sensor_slot_up;
    const char *slot_down = config->sensor_slot_down;
    const int up_active = slot_is_active(slot_up);
    const int down_active = slot_is_active(slot_down);

    // Get labels from slots (NULL if "none")
    const char *label_up = get_slot_label(slot_up);
    const char *label_down = get_slot_label(slot_down);

    // Get bar heights
    const uint16_t bar_height_up = get_slot_bar_height(config, "up");
    const uint16_t bar_height_down = get_slot_bar_height(config, "down");

    // Calculate total height based on active slots
    int total_height = 0;
    if (up_active && down_active)
        total_height = bar_height_up + config->layout_bar_gap + bar_height_down;
    else if (up_active)
        total_height = bar_height_up;
    else if (down_active)
        total_height = bar_height_down;
    else
        return;

    const int start_y = (config->display_height - total_height) / 2;
    int up_bar_y = start_y;
    int down_bar_y = start_y + bar_height_up + config->layout_bar_gap;

    // If only down slot is active, center it
    if (!up_active && down_active)
        down_bar_y = start_y;

    // Labels: Configurable distance from left screen edge (default: 1%)
    const double left_margin_factor =
        (config->layout_label_margin_left > 0)
            ? (config->layout_label_margin_left / 100.0)
            : 0.01;
    double label_x = config->display_width * left_margin_factor;

    // Apply user-defined X offset if set
    if (config->display_label_offset_x != -9999)
    {
        label_x += config->display_label_offset_x;
    }

    cairo_font_extents_t font_ext;
    cairo_font_extents(cr, &font_ext);

    // Vertical spacing: Configurable distance from bars (default: 1%)
    const double bar_margin_factor =
        (config->layout_label_margin_bar > 0)
            ? (config->layout_label_margin_bar / 100.0)
            : 0.01;
    const double label_spacing = config->display_height * bar_margin_factor;

    // Draw upper slot label (if active and has label)
    if (up_active && label_up)
    {
        double up_label_y = up_bar_y - label_spacing - font_ext.descent;
        if (config->display_label_offset_y != -9999)
            up_label_y += config->display_label_offset_y;

        cairo_move_to(cr, label_x, up_label_y);
        cairo_show_text(cr, label_up);
    }

    // Draw lower slot label (if active and has label)
    if (down_active && label_down)
    {
        double down_label_y = down_bar_y + bar_height_down + label_spacing + font_ext.ascent;
        if (config->display_label_offset_y != -9999)
            down_label_y += config->display_label_offset_y;

        cairo_move_to(cr, label_x, down_label_y);
        cairo_show_text(cr, label_down);
    }
}

/**
 * @brief Render display content to cairo context
 */
static void render_display_content(cairo_t *cr, const struct Config *config,
                                   const monitor_sensor_data_t *data,
                                   const ScalingParams *params)
{
    // Draw main background
    set_cairo_color(cr, &config->display_background_color);
    cairo_paint(cr);

    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    set_cairo_color(cr, &config->font_color_temp);

    draw_temperature_displays(cr, data, config, params);
    draw_temperature_bars(cr, data, config, params);

    // Get temperatures from active slots for label visibility check
    float temp_up = get_slot_temperature(data, config->sensor_slot_up);
    float temp_down = get_slot_temperature(data, config->sensor_slot_down);

    // Labels only if both temps < 99°C (to avoid overlap with large numbers)
    if (temp_up < 99.0f && temp_down < 99.0f)
    {
        cairo_set_font_size(cr, config->font_size_labels);
        set_cairo_color(cr, &config->font_color_label);
        draw_labels(cr, config, data, params);
    }
}

/**
 * @brief Display rendering - creates surface, renders content, saves PNG (Dual
 * mode - CPU+GPU)
 */
static int render_dual_display(const struct Config *config,
                               const monitor_sensor_data_t *data,
                               const char *device_name)
{
    if (!data || !config)
    {
        log_message(LOG_ERROR, "Invalid parameters for render_display");
        return 0;
    }

    ScalingParams scaling_params;
    calculate_scaling_params(config, &scaling_params, device_name);

    // Log display shape detection
    if (scaling_params.is_circular)
    {
        log_message(LOG_INFO,
                    "Circular display detected (device: %s, inscribe factor: %.4f)",
                    device_name ? device_name : "unknown",
                    scaling_params.inscribe_factor);
    }
    else
    {
        log_message(
            LOG_INFO,
            "Rectangular display detected (device: %s, inscribe factor: %.4f)",
            device_name ? device_name : "unknown", scaling_params.inscribe_factor);
    }

    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;

    render_display_content(cr, config, data, &scaling_params);

    cairo_surface_flush(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Cairo drawing error: %s",
                    cairo_status_to_string(cairo_status(cr)));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    cairo_status_t write_status =
        cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    int success = (write_status == CAIRO_STATUS_SUCCESS);

    if (!success)
        log_message(LOG_ERROR, "Failed to write PNG image: %s",
                    config->paths_image_coolerdash);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return success;
}

/**
 * @brief Main dual mode entry point.
 * @details Collects sensor data, renders dual display using
 * render_dual_display(), and sends to LCD device.
 */
void draw_dual_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config parameter");
        return;
    }

    // Get sensor data
    monitor_sensor_data_t sensor_data = {.temp_cpu = 0.0f, .temp_gpu = 0.0f};
    if (!get_temperature_monitor_data(config, &sensor_data))
    {
        log_message(LOG_WARNING, "Failed to retrieve temperature data");
        return;
    }

    // Get LCD device info
    char device_uid[128] = {0};
    char device_name[256] = {0};
    int screen_width = 0, screen_height = 0;

    const bool device_available =
        get_liquidctl_data(config, device_uid, sizeof(device_uid), device_name,
                           sizeof(device_name), &screen_width, &screen_height);

    // Render dual display with device name for circular display detection
    if (!render_dual_display(config, &sensor_data, device_name))
    {
        log_message(LOG_ERROR, "Dual display rendering failed");
        return;
    }

    // Send to LCD if available
    if (is_session_initialized() && device_available && device_uid[0] != '\0')
    {
        const char *name =
            (device_name[0] != '\0') ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending dual image to LCD: %s [%s]", name,
                    device_uid);

        // Send image to LCD device
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);

        log_message(LOG_INFO, "Dual LCD image uploaded successfully");
    }
    else
    {
        log_message(LOG_WARNING, "Skipping dual LCD upload - device not available");
    }
}
