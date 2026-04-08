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
 * @brief Forward declarations for internal display rendering functions.
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

typedef struct
{
    int up_active;
    int down_active;
    int bar_x;
    int effective_bar_width;
    int bar_gap;
    int up_bar_y;
    int down_bar_y;
    uint16_t bar_height_up;
    uint16_t bar_height_down;
    double label_spacing;
    double top_value_box_y;
    double top_value_box_height;
    double bottom_value_box_y;
    double bottom_value_box_height;
} DualLayout;

static int calculate_dual_layout(const struct Config *config,
                                 const ScalingParams *params,
                                 DualLayout *layout)
{
    if (!config || !params || !layout)
        return 0;

    memset(layout, 0, sizeof(*layout));

    layout->effective_bar_width = params->safe_bar_width;
    layout->bar_x = (int)lround(params->safe_content_margin);
    layout->up_active = slot_is_active(config->sensor_slot_1);
    layout->down_active = slot_is_active(config->sensor_slot_3);
    layout->bar_height_up = (uint16_t)get_scaled_slot_bar_height(config, params, "1");
    layout->bar_height_down = (uint16_t)get_scaled_slot_bar_height(config, params, "3");
    layout->bar_gap = get_scaled_bar_gap(config, params);

    int total_height = 0;
    if (layout->up_active && layout->down_active)
        total_height = layout->bar_height_up + layout->bar_gap +
                       layout->bar_height_down;
    else if (layout->up_active)
        total_height = layout->bar_height_up;
    else if (layout->down_active)
        total_height = layout->bar_height_down;
    else
        return 0;

    const int start_y =
        (int)lround(((double)config->display_height - total_height) / 2.0);
    layout->up_bar_y = start_y;
    layout->down_bar_y = start_y + layout->bar_height_up +
                         layout->bar_gap;

    if (!layout->up_active && layout->down_active)
        layout->down_bar_y = start_y;

    layout->label_spacing = get_effective_label_spacing(config, params);
    const double value_bar_gap = layout->label_spacing * 0.05;

    layout->top_value_box_y = 0.0;
    layout->top_value_box_height =
        fmax(0.0, layout->up_bar_y - value_bar_gap);
    layout->bottom_value_box_y =
        layout->down_bar_y + layout->bar_height_down + value_bar_gap;
    layout->bottom_value_box_height =
        fmax(0.0, config->display_height - layout->bottom_value_box_y);

    if (verbose_logging)
    {
        log_message(
            LOG_INFO,
            "Dual layout: logical(up=%u, down=%u, gap=%u) scaled(up=%u, down=%u, gap=%d) start_y=%d up_y=%d down_y=%d label_spacing=%.1f value_gap=%.1f safe_width=%d margin=%.1f",
            get_slot_bar_height(config, "1"),
            get_slot_bar_height(config, "3"), config->layout_bar_gap,
            layout->bar_height_up, layout->bar_height_down, layout->bar_gap,
            start_y, layout->up_bar_y, layout->down_bar_y,
            layout->label_spacing, value_bar_gap, layout->effective_bar_width,
            params->safe_content_margin);
    }

    return 1;
}

/**
 * @brief Draw temperature displays for up and down slots.
 */
static void draw_temperature_displays(cairo_t *cr,
                                      const monitor_sensor_data_t *data,
                                      const struct Config *config,
                                      const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    DualLayout layout = {0};
    if (!calculate_dual_layout(config, params, &layout))
        return;

    const char *slot_up = config->sensor_slot_1;
    const char *slot_down = config->sensor_slot_3;

    float temp_up = get_slot_temperature(data, slot_up);
    float temp_down = get_slot_temperature(data, slot_down);

    if (layout.up_active && layout.top_value_box_height > 0.0)
    {
        double safe_x = layout.bar_x;
        double safe_width = layout.effective_bar_width;
        calculate_text_lane_bounds(config, params, layout.top_value_box_y,
                                   layout.top_value_box_height, 0,
                                   layout.bar_x,
                                   layout.effective_bar_width, &safe_x,
                                   &safe_width);
        SlotValueLayout up_layout;
        layout_and_render_slot_value(cr, data, config, params, slot_up,
                                     temp_up, safe_x,
                                     layout.top_value_box_y,
                                     safe_width,
                                     layout.top_value_box_height, 0, 1,
                                     &up_layout);
    }

    if (layout.down_active && layout.bottom_value_box_height > 0.0)
    {
        double safe_x = layout.bar_x;
        double safe_width = layout.effective_bar_width;
        calculate_text_lane_bounds(config, params, layout.bottom_value_box_y,
                                   layout.bottom_value_box_height,
                                   1,
                                   layout.bar_x, layout.effective_bar_width,
                                   &safe_x, &safe_width);
        SlotValueLayout down_layout;
        layout_and_render_slot_value(cr, data, config, params, slot_down,
                                     temp_down, safe_x,
                                     layout.bottom_value_box_y,
                                     safe_width,
                                     layout.bottom_value_box_height, 1, 1,
                                     &down_layout);
    }
}

/**
 * @brief Draw a single temperature bar with background, fill, and border.
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
    const double bar_alpha = config->layout_bar_opacity;

    // Background
    set_cairo_color_alpha(cr, &config->layout_bar_color_background, bar_alpha);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, bar_width,
                                bar_height, params->corner_radius);
    cairo_fill(cr);

    // Fill
    if (fill_width > 0)
    {
        Color fill_color = get_slot_bar_color(config, slot_value, temp_value);
        set_cairo_color_alpha(cr, &fill_color, bar_alpha);

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
        cairo_set_line_width(cr, get_scaled_bar_border_width(config, params));
        set_cairo_color_alpha(cr, &config->layout_bar_color_border, bar_alpha);
        draw_rounded_rectangle_path(cr, bar_x, bar_y, bar_width,
                                    bar_height, params->corner_radius);
        cairo_stroke(cr);
    }
}

/**
 * @brief Draw temperature bars for up and down slots.
 */
static void draw_temperature_bars(cairo_t *cr,
                                  const monitor_sensor_data_t *data,
                                  const struct Config *config,
                                  const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    DualLayout layout = {0};
    if (!calculate_dual_layout(config, params, &layout))
        return;

    const char *slot_up = config->sensor_slot_1;
    const char *slot_down = config->sensor_slot_3;

    if (layout.up_active)
    {
        float temp_up = get_slot_temperature(data, slot_up);
        draw_single_temperature_bar_slot(cr, config, params, slot_up, temp_up,
                                         layout.bar_x, layout.up_bar_y,
                                         layout.effective_bar_width,
                                         layout.bar_height_up);
    }

    if (layout.down_active)
    {
        float temp_down = get_slot_temperature(data, slot_down);
        draw_single_temperature_bar_slot(cr, config, params, slot_down, temp_down,
                                         layout.bar_x, layout.down_bar_y,
                                         layout.effective_bar_width,
                                         layout.bar_height_down);
    }
}

/**
 * @brief Draw labels for up and down slots.
 */
static void draw_labels(cairo_t *cr, const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params)
{
    if (!cr || !config || !params)
        return;

    DualLayout layout = {0};
    if (!calculate_dual_layout(config, params, &layout))
        return;

    const char *slot_up = config->sensor_slot_1;
    const char *slot_down = config->sensor_slot_3;

    const char *label_up = get_slot_label(config, data, slot_up);
    const char *label_down = get_slot_label(config, data, slot_down);

    SlotValueLayout up_layout = {0};
    SlotValueLayout down_layout = {0};
    if (layout.up_active && layout.top_value_box_height > 0.0)
    {
        double safe_x = layout.bar_x;
        double safe_width = layout.effective_bar_width;
        calculate_text_lane_bounds(config, params, layout.top_value_box_y,
                                   layout.top_value_box_height, 0,
                                   layout.bar_x,
                                   layout.effective_bar_width, &safe_x,
                                   &safe_width);
        layout_and_render_slot_value(cr, data, config, params, slot_up,
                                     get_slot_temperature(data, slot_up),
                                     safe_x, layout.top_value_box_y,
                                     safe_width,
                                     layout.top_value_box_height, 0, 0,
                                     &up_layout);
    }
    if (layout.down_active && layout.bottom_value_box_height > 0.0)
    {
        double safe_x = layout.bar_x;
        double safe_width = layout.effective_bar_width;
        calculate_text_lane_bounds(config, params, layout.bottom_value_box_y,
                                   layout.bottom_value_box_height,
                                   1,
                                   layout.bar_x, layout.effective_bar_width,
                                   &safe_x, &safe_width);
        layout_and_render_slot_value(cr, data, config, params, slot_down,
                                     get_slot_temperature(data, slot_down),
                                     safe_x, layout.bottom_value_box_y,
                                     safe_width,
                                     layout.bottom_value_box_height, 1, 0,
                                     &down_layout);
    }

    // Labels: Configurable distance from left screen edge (default: 1%)
    const double left_margin_factor =
        (config->layout_label_margin_left > 0)
            ? (config->layout_label_margin_left / 100.0)
            : 0.01;
    double label_x = params->safe_content_margin +
                     (layout.effective_bar_width * left_margin_factor);

    // Apply user-defined X offset if set
    label_x += get_scaled_label_offset_x(config, params);

    cairo_font_extents_t font_ext;
    cairo_font_extents(cr, &font_ext);

    // Draw upper slot label (if active and has label)
    if (layout.up_active && label_up)
    {
        double label_font_size = get_preferred_label_font_size(config, params);
        const double min_label_font_size =
            (config->font_size_labels > 0.0f)
                ? fmax(10.0, scale_value_avg(params,
                                             (double)config->font_size_labels) *
                                 0.60)
                : fmax(10.0, scale_value_avg(params, 10.0));
        cairo_text_extents_t up_label_ext;
        while (1)
        {
            cairo_set_font_size(cr, label_font_size);
            cairo_font_extents(cr, &font_ext);
            cairo_text_extents(cr, label_up, &up_label_ext);

            const double up_safe_right = up_layout.active
                                             ? (up_layout.block_left - scale_value_avg(params, 8.0))
                                             : (layout.bar_x + layout.effective_bar_width);
            const double up_available_width = fmax(8.0, up_safe_right - label_x);

            if (fmax(up_label_ext.x_advance, up_label_ext.width) <= up_available_width ||
                label_font_size <= min_label_font_size)
                break;

            label_font_size *= 0.92;
            if (label_font_size < min_label_font_size)
                label_font_size = min_label_font_size;
        }

        double up_label_y = layout.up_bar_y - layout.label_spacing - font_ext.descent;
        up_label_y += get_scaled_label_offset_y(config, params);

        const double up_label_right = label_x + fmax(up_label_ext.x_advance, up_label_ext.width);
        const double up_safe_right = up_layout.active
                                         ? (up_layout.block_left - scale_value_avg(params, 8.0))
                                         : (layout.bar_x + layout.effective_bar_width);
        if (up_label_right <= up_safe_right)
        {
            cairo_set_font_size(cr, label_font_size);
            cairo_move_to(cr, label_x, up_label_y);
            cairo_show_text(cr, label_up);
        }
    }

    // Draw lower slot label (if active and has label)
    if (layout.down_active && label_down)
    {
        double label_font_size = get_preferred_label_font_size(config, params);
        const double min_label_font_size =
            (config->font_size_labels > 0.0f)
                ? fmax(10.0, scale_value_avg(params,
                                             (double)config->font_size_labels) *
                                 0.60)
                : fmax(10.0, scale_value_avg(params, 10.0));
        cairo_text_extents_t down_label_ext;
        while (1)
        {
            cairo_set_font_size(cr, label_font_size);
            cairo_font_extents(cr, &font_ext);
            cairo_text_extents(cr, label_down, &down_label_ext);

            const double down_safe_right = down_layout.active
                                               ? (down_layout.block_left - scale_value_avg(params, 8.0))
                                               : (layout.bar_x + layout.effective_bar_width);
            const double down_available_width =
                fmax(8.0, down_safe_right - label_x);

            if (fmax(down_label_ext.x_advance, down_label_ext.width) <=
                    down_available_width ||
                label_font_size <= min_label_font_size)
                break;

            label_font_size *= 0.92;
            if (label_font_size < min_label_font_size)
                label_font_size = min_label_font_size;
        }

        double down_label_y = layout.down_bar_y + layout.bar_height_down +
                              layout.label_spacing + font_ext.ascent;
        down_label_y += get_scaled_label_offset_y(config, params);

        const double down_label_right =
            label_x + fmax(down_label_ext.x_advance, down_label_ext.width);
        const double down_safe_right = down_layout.active
                                           ? (down_layout.block_left - scale_value_avg(params, 8.0))
                                           : (layout.bar_x + layout.effective_bar_width);
        if (down_label_right <= down_safe_right)
        {
            cairo_set_font_size(cr, label_font_size);
            cairo_move_to(cr, label_x, down_label_y);
            cairo_show_text(cr, label_down);
        }
    }
}

/**
 * @brief Render display content to cairo context.
 */
static void render_display_content(cairo_t *cr, const struct Config *config,
                                   const monitor_sensor_data_t *data,
                                   const ScalingParams *params)
{
    paint_display_background(cr, config);

    draw_temperature_bars(cr, data, config, params);

    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    set_cairo_color(cr, &config->font_color_temp);

    draw_temperature_displays(cr, data, config, params);

    cairo_set_font_size(cr, config->font_size_labels);
    set_cairo_color(cr, &config->font_color_label);
    draw_labels(cr, config, data, params);
}

/**
 * @brief Display rendering - creates surface, renders content, saves PNG (Dual
 * mode - CPU+GPU).
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
    monitor_sensor_data_t sensor_data = {0};
    if (!get_sensor_monitor_data(config, &sensor_data))
    {
        log_message(LOG_WARNING, "Failed to retrieve sensor data");
        return;
    }

    // Get LCD device info
    char device_uid[128] = {0};
    char device_name[256] = {0};
    int screen_width = 0, screen_height = 0;

    const bool device_available =
        get_cached_lcd_device_data(config, device_uid, sizeof(device_uid),
                                   device_name, sizeof(device_name),
                                   &screen_width, &screen_height);

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
