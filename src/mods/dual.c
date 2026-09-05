/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
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
    int up_bar_x;
    int up_bar_width;
    int down_bar_x;
    int down_bar_width;
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

typedef struct
{
    double label_x;
    double label_width;
    double label_font_size;
    double value_x;
    double value_width;
} DualTextRow;

#define DUAL_LABEL_TO_VALUE_RATIO (1.0 / 3.0)

static double get_dual_minimum_spacing(const struct Config *config,
                                       const ScalingParams *params)
{
    const double min_dimension =
        fmin((double)config->display_width,
             (double)config->display_height);
    return fmax(scale_value_avg(params, 1.0), min_dimension * 0.04);
}

static int calculate_dual_text_row(cairo_t *cr, const struct Config *config,
                                   const ScalingParams *params,
                                   const char *label, double box_y,
                                   double box_height, int align_bottom,
                                   int bar_x, int bar_width,
                                   double preferred_label_size,
                                   DualTextRow *row)
{
    if (!cr || !config || !params || !label || !row || box_height <= 0.0)
        return 0;

    double safe_x = bar_x;
    double safe_width = bar_width;
    calculate_text_lane_bounds(config, params, box_y, box_height,
                               align_bottom, bar_x, bar_width,
                               &safe_x, &safe_width);
    if (!config->dual_show_bars)
    {
        const double band_height = box_height * 0.65;
        const double band_y = align_bottom ? box_y + box_height - band_height : box_y;
        calculate_safe_region_bounds(params, band_y, band_height, 0.96,
                                     bar_x, bar_width, &safe_x, &safe_width);
    }

    const double left_margin_factor =
        (config->layout_label_margin_left > 0)
            ? config->layout_label_margin_left / 100.0
            : 0.01;
    row->label_x = safe_x + safe_width * left_margin_factor +
                   get_scaled_label_offset_x(config, params);
    row->label_x = fmax(safe_x, row->label_x);
    const double element_gap = get_dual_minimum_spacing(config, params);
    const double available_label_width =
        fmax(1.0, safe_x + safe_width - row->label_x - element_gap - 1.0);
    row->label_font_size = fit_text_font_size(
        cr, label, preferred_label_size,
        available_label_width, box_height, 1.0);

    cairo_text_extents_t label_ext = {0};
    cairo_set_font_size(cr, row->label_font_size);
    cairo_text_extents(cr, label, &label_ext);
    row->label_width = fmax(label_ext.x_advance, label_ext.width);
    row->value_x = row->label_x + row->label_width + element_gap;
    row->value_width = fmax(1.0, safe_x + safe_width - row->value_x);
    return 1;
}

/** @brief Jointly fit a Dual label and value while preserving a 1:3 size ratio. */
static int calculate_dual_sensor_row(
    cairo_t *cr, const monitor_sensor_data_t *data,
    const struct Config *config, const ScalingParams *params,
    const char *slot_value, const char *label, float temp_value,
    double box_y, double box_height, int align_bottom,
    int bar_x, int bar_width, DualTextRow *row,
    SlotValueLayout *value_layout)
{
    if (!cr || !data || !config || !params || !slot_value || !label ||
        !row || !value_layout)
        return 0;

    double label_size = fmax(1.0, box_height * 0.5);
    for (int pass = 0; pass < 24; pass++)
    {
        if (!calculate_dual_text_row(cr, config, params, label, box_y,
                                     box_height, align_bottom, bar_x,
                                     bar_width, label_size, row))
            return 0;

        layout_and_render_slot_value(cr, data, config, params, slot_value,
                                     temp_value, row->value_x, box_y,
                                     row->value_width, box_height,
                                     align_bottom, 0, value_layout);
        if (!value_layout->active)
            return 0;
        const double target_label_size =
            value_layout->font_size * DUAL_LABEL_TO_VALUE_RATIO;
        if (fabs(row->label_font_size - target_label_size) < 0.05)
            return 1;
        label_size = target_label_size;
    }

    return 1;
}

static int calculate_dual_layout(const struct Config *config,
                                 const ScalingParams *params,
                                 DualLayout *layout)
{
    if (!config || !params || !layout)
        return 0;

    memset(layout, 0, sizeof(*layout));

    LayoutContext geometry = {0};
    if (!calculate_layout_context(config, params, &geometry))
        return 0;

    layout->up_bar_width = params->safe_bar_width;
    layout->down_bar_width = params->safe_bar_width;
    layout->up_bar_x = (int)lround(params->safe_content_margin);
    layout->down_bar_x = layout->up_bar_x;
    layout->up_active = slot_is_active(config->sensor_slot_1);
    layout->down_active = slot_is_active(config->sensor_slot_3);
    layout->bar_height_up =
        (uint16_t)get_scaled_slot_bar_height(config, params, "1");
    layout->bar_height_down =
        (uint16_t)get_scaled_slot_bar_height(config, params, "3");
    layout->bar_gap = get_scaled_bar_gap(config, params);
    if (!config->dual_show_bars)
    {
        layout->bar_height_up = 0;
        layout->bar_height_down = 0;
        layout->bar_gap = 0;
    }

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
        (int)lround(geometry.center_y - total_height / 2.0);
    layout->up_bar_y = start_y;
    layout->down_bar_y = start_y + layout->bar_height_up +
                         layout->bar_gap;

    if (!layout->up_active && layout->down_active)
        layout->down_bar_y = start_y;

    if (layout->up_active)
        calculate_bar_bounds(config, params, layout->up_bar_y,
                             layout->bar_height_up, &layout->up_bar_x,
                             &layout->up_bar_width);
    if (layout->down_active)
        calculate_bar_bounds(config, params, layout->down_bar_y,
                             layout->bar_height_down, &layout->down_bar_x,
                             &layout->down_bar_width);

    layout->label_spacing =
        fmax(get_effective_label_spacing(config, params),
             get_dual_minimum_spacing(config, params));
    const double value_bar_gap = layout->label_spacing;

    const SensorConfig *sc_up = get_sensor_config(config, config->sensor_slot_1);
    const SensorConfig *sc_down = get_sensor_config(config, config->sensor_slot_3);
    const double dual_avail_height = geometry.height;
    const double up_value_gap =
        (sc_up && sc_up->value_to_bar_gap > 0.0f)
            ? fmax(value_bar_gap,
                   dual_avail_height * (sc_up->value_to_bar_gap / 100.0))
            : value_bar_gap;
    const double up_label_gap =
        (sc_up && sc_up->label_to_bar_gap > 0.0f)
            ? fmax(value_bar_gap,
                   dual_avail_height * (sc_up->label_to_bar_gap / 100.0))
            : value_bar_gap;
    const double down_value_gap =
        (sc_down && sc_down->value_to_bar_gap > 0.0f)
            ? fmax(value_bar_gap,
                   dual_avail_height * (sc_down->value_to_bar_gap / 100.0))
            : value_bar_gap;
    const double down_label_gap =
        (sc_down && sc_down->label_to_bar_gap > 0.0f)
            ? fmax(value_bar_gap,
                   dual_avail_height * (sc_down->label_to_bar_gap / 100.0))
            : value_bar_gap;
    const double gap_above_top = fmax(up_value_gap, up_label_gap);
    const double gap_below_bottom = fmax(down_value_gap, down_label_gap);

    layout->top_value_box_y = geometry.top;
    layout->top_value_box_height =
        fmax(0.0, layout->up_bar_y - gap_above_top - geometry.top);
    layout->bottom_value_box_y =
        layout->down_bar_y + layout->bar_height_down + gap_below_bottom;
    layout->bottom_value_box_height =
        fmax(0.0, geometry.bottom - layout->bottom_value_box_y);

    if (verbose_logging)
    {
        log_message(
            LOG_INFO,
            "Dual layout: logical(up=%u down=%u gap=%u) "
            "scaled(up=%u, down=%u, gap=%d) start_y=%d up_y=%d down_y=%d "
            "label_spacing=%.1f value_gap=%.1f bar_widths=(%d,%d)",
            get_slot_bar_height(config, "1"),
            get_slot_bar_height(config, "3"), config->layout_bar_gap,
            layout->bar_height_up, layout->bar_height_down, layout->bar_gap,
            start_y, layout->up_bar_y, layout->down_bar_y,
            layout->label_spacing, value_bar_gap, layout->up_bar_width,
            layout->down_bar_width);
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
    const SensorConfig *sc_up = get_sensor_config(config, slot_up);
    const SensorConfig *sc_down = get_sensor_config(config, slot_down);
    const char *label_up = (sc_up && sc_up->label[0] != '\0')
                               ? sc_up->label
                               : "CPU";
    const char *label_down = (sc_down && sc_down->label[0] != '\0')
                                 ? sc_down->label
                                 : "GPU";

    float temp_up = get_slot_temperature(data, slot_up);
    float temp_down = get_slot_temperature(data, slot_down);

    if (layout.up_active && layout.top_value_box_height > 0.0)
    {
        DualTextRow row = {0};
        SlotValueLayout up_layout = {0};
        if (!calculate_dual_sensor_row(
                cr, data, config, params, slot_up, label_up, temp_up,
                layout.top_value_box_y, layout.top_value_box_height, 1,
                layout.up_bar_x, layout.up_bar_width, &row, &up_layout))
            return;
        layout_and_render_slot_value(cr, data, config, params, slot_up,
                                     temp_up, row.value_x,
                                     layout.top_value_box_y,
                                     row.value_width,
                                     layout.top_value_box_height, 1, 1,
                                     &up_layout);
    }

    if (layout.down_active && layout.bottom_value_box_height > 0.0)
    {
        DualTextRow row = {0};
        SlotValueLayout down_layout = {0};
        if (!calculate_dual_sensor_row(
                cr, data, config, params, slot_down, label_down, temp_down,
                layout.bottom_value_box_y,
                layout.bottom_value_box_height, 0,
                layout.down_bar_x, layout.down_bar_width, &row,
                &down_layout))
            return;
        layout_and_render_slot_value(cr, data, config, params, slot_down,
                                     temp_down, row.value_x,
                                     layout.bottom_value_box_y,
                                     row.value_width,
                                     layout.bottom_value_box_height, 0, 1,
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
                                         layout.up_bar_x, layout.up_bar_y,
                                         layout.up_bar_width,
                                         layout.bar_height_up);
    }

    if (layout.down_active)
    {
        float temp_down = get_slot_temperature(data, slot_down);
        draw_single_temperature_bar_slot(cr, config, params, slot_down, temp_down,
                                         layout.down_bar_x, layout.down_bar_y,
                                         layout.down_bar_width,
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

    /* Dual mode: use custom label if set, otherwise always "CPU" / "GPU" */
    const SensorConfig *sc_lbl_up = get_sensor_config(config, slot_up);
    const SensorConfig *sc_lbl_down = get_sensor_config(config, slot_down);
    const char *label_up = (sc_lbl_up && sc_lbl_up->label[0] != '\0')
                               ? sc_lbl_up->label
                               : "CPU";
    const char *label_down = (sc_lbl_down && sc_lbl_down->label[0] != '\0')
                                 ? sc_lbl_down->label
                                 : "GPU";

    SlotValueLayout up_layout = {0};
    SlotValueLayout down_layout = {0};
    DualTextRow up_row = {0};
    DualTextRow down_row = {0};
    if (layout.up_active && layout.top_value_box_height > 0.0)
    {
        if (!calculate_dual_sensor_row(
                cr, data, config, params, slot_up, label_up,
                get_slot_temperature(data, slot_up),
                layout.top_value_box_y, layout.top_value_box_height, 1,
                layout.up_bar_x, layout.up_bar_width, &up_row,
                &up_layout))
            return;
    }
    if (layout.down_active && layout.bottom_value_box_height > 0.0)
    {
        if (!calculate_dual_sensor_row(
                cr, data, config, params, slot_down, label_down,
                get_slot_temperature(data, slot_down),
                layout.bottom_value_box_y,
                layout.bottom_value_box_height, 0,
                layout.down_bar_x, layout.down_bar_width, &down_row,
                &down_layout))
            return;
    }

    // Draw upper slot label (if active and has label)
    if (layout.up_active && label_up)
    {
        cairo_text_extents_t ext = {0};
        cairo_set_font_size(cr, up_row.label_font_size);
        cairo_text_extents(cr, label_up, &ext);
        const double box_bottom = layout.top_value_box_y +
                                  layout.top_value_box_height;
        double up_label_y = box_bottom - ext.y_bearing - ext.height +
                            get_scaled_label_offset_y(config, params);
        up_label_y = fmin(up_label_y,
                          box_bottom - ext.y_bearing - ext.height);
        cairo_move_to(cr, up_row.label_x - ext.x_bearing, up_label_y);
        cairo_show_text(cr, label_up);
    }

    // Draw lower slot label (if active and has label)
    if (layout.down_active && label_down)
    {
        cairo_text_extents_t ext = {0};
        cairo_set_font_size(cr, down_row.label_font_size);
        cairo_text_extents(cr, label_down, &ext);
        double down_label_y = layout.bottom_value_box_y - ext.y_bearing +
                              get_scaled_label_offset_y(config, params);
        down_label_y = fmax(down_label_y,
                            layout.bottom_value_box_y - ext.y_bearing);
        cairo_move_to(cr, down_row.label_x - ext.x_bearing, down_label_y);
        cairo_show_text(cr, label_down);
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

    if (config->dual_show_bars)
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
                    "Circular display detected (device: %s, profile: %s)",
                    device_name ? device_name : "unknown",
                    scaling_params.profile_name);
    }
    else
    {
        log_message(
            LOG_INFO,
            "Rectangular display detected (device: %s, profile: %s)",
            device_name ? device_name : "unknown", scaling_params.profile_name);
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
