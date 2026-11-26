/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

// cppcheck-suppress-begin missingIncludeSystem
#include <math.h>
#include <pthread.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

#include "dual.h"
#include "ring.h"
#include "text.h"
#include "bar.h"
#include "render.h"

enum
{
    DUAL_UP,
    DUAL_DOWN,
    DUAL_ROW_COUNT
};

enum
{
    DUAL_LABEL,
    DUAL_VALUE,
    DUAL_BAR,
    DUAL_WIDGETS_PER_ROW
};

static const char *const dual_sources[] = {"cpu", "gpu"};
static const CustomScreen *dual_screen;
static pthread_once_t dual_preset_once = PTHREAD_ONCE_INIT;

static void load_dual_preset(void)
{
    static CustomLayout preset;
    if (!load_layout_preset("dual", &preset))
        return;

    const CustomScreen *screen = &preset.screens[0];
    if (preset.screen_count == 1 && screen->count == DUAL_ROW_COUNT * DUAL_WIDGETS_PER_ROW)
    {
        for (int i = 0; i < DUAL_ROW_COUNT; i++)
        {
            const CustomElement *widgets = &screen->elements[i * DUAL_WIDGETS_PER_ROW];
            if (widgets[DUAL_LABEL].type != CUSTOM_TEXT ||
                widgets[DUAL_VALUE].type != CUSTOM_VALUE ||
                widgets[DUAL_BAR].type != CUSTOM_BAR ||
                strcmp(widgets[DUAL_VALUE].source, dual_sources[i]) != 0 ||
                strcmp(widgets[DUAL_BAR].source, dual_sources[i]) != 0)
            {
                log_message(LOG_ERROR, "Invalid row in the Dual layout preset");
                return;
            }
        }
        dual_screen = screen;
    }
    else
        log_message(LOG_ERROR, "Dual layout preset requires two rows on one screen");
}

typedef struct
{
    const char *slot;
    const char *label;
    float temperature;
    float max_scale;
    Color bar_color;
    int active;
    int align_bottom;
    int bar_x, bar_y, bar_width;
    uint16_t bar_height;
    double box_y, box_height;
    double label_x, label_size;
    double label_to_value_ratio;
    double value_x, value_width;
} DualRow;

static double get_dual_minimum_spacing(const Config *config,
                                       const ScalingParams *params)
{
    const double min_dimension = fmin(config->display_width, config->display_height);
    return fmax(scale_value_avg(params, 1.0), min_dimension * 0.04);
}

static int calculate_dual_layout(const Config *config,
                                 const monitor_sensor_data_t *data,
                                 const ScalingParams *params,
                                 DualRow rows[DUAL_ROW_COUNT])
{
    LayoutContext geometry = {0};
    if (!calculate_layout_context(config, params, &geometry))
        return 0;

    const char *const slots[] = {config->sensor_slot_1, config->sensor_slot_3};
    const char *const slot_numbers[] = {"1", "3"};
    const int bar_gap = config->dual_show_bars ? get_scaled_bar_gap(config, params) : 0;
    int total_height = 0;
    for (int i = 0; i < DUAL_ROW_COUNT; i++)
    {
        DualRow *row = &rows[i];
        row->slot = slots[i];
        row->active = slot_is_active(row->slot);
        row->align_bottom = i == DUAL_UP;
        row->bar_width = params->safe_bar_width;
        row->bar_x = (int)lround(params->safe_content_margin);
        row->bar_height = config->dual_show_bars
                              ? (uint16_t)get_scaled_slot_bar_height(config, params, slot_numbers[i])
                              : 0;
        if (row->active)
            total_height += row->bar_height;
    }

    if (!rows[DUAL_UP].active && !rows[DUAL_DOWN].active)
        return 0;
    if (rows[DUAL_UP].active && rows[DUAL_DOWN].active)
        total_height += bar_gap;

    const int start_y = (int)lround(geometry.center_y - total_height / 2.0);
    rows[DUAL_UP].bar_y = start_y;
    rows[DUAL_DOWN].bar_y = start_y;
    if (rows[DUAL_UP].active)
        rows[DUAL_DOWN].bar_y += rows[DUAL_UP].bar_height + bar_gap;

    const double label_spacing =
        fmax(get_effective_label_spacing(config, params), get_dual_minimum_spacing(config, params));
    for (int i = 0; i < DUAL_ROW_COUNT; i++)
    {
        DualRow *row = &rows[i];
        if (!row->active)
            continue;

        const CustomElement *widgets = &dual_screen->elements[i * DUAL_WIDGETS_PER_ROW];
        const SensorConfig *sensor = get_sensor_config(config, row->slot);
        row->label = sensor && sensor->label[0] ? sensor->label : widgets[DUAL_LABEL].text;
        row->label_to_value_ratio = (double)widgets[DUAL_LABEL].font_size / widgets[DUAL_VALUE].font_size;
        row->temperature = get_slot_temperature(data, row->slot);
        row->max_scale = sensor && sensor->max_scale > 0.0f
                             ? sensor->max_scale : (float)widgets[DUAL_BAR].maximum;
        row->bar_color = sensor ? get_slot_bar_color(config, row->slot, row->temperature)
                                : widgets[DUAL_BAR].color;
        calculate_bar_bounds(config, params, row->bar_y, row->bar_height,
                             &row->bar_x, &row->bar_width);

        double gap = label_spacing;
        if (sensor && sensor->value_to_bar_gap > 0.0f)
            gap = fmax(gap, geometry.height * (sensor->value_to_bar_gap / 100.0));
        if (sensor && sensor->label_to_bar_gap > 0.0f)
            gap = fmax(gap, geometry.height * (sensor->label_to_bar_gap / 100.0));

        if (row->align_bottom)
        {
            row->box_y = geometry.top;
            row->box_height = fmax(0.0, row->bar_y - gap - geometry.top);
        }
        else
        {
            row->box_y = row->bar_y + row->bar_height + gap;
            row->box_height = fmax(0.0, geometry.bottom - row->box_y);
        }
    }

    return 1;
}

static void calculate_dual_text_row(cairo_t *cr, const Config *config,
                                    const ScalingParams *params, double preferred_size,
                                    DualRow *row)
{
    double safe_x = row->bar_x;
    double safe_width = row->bar_width;
    calculate_text_lane_bounds(config, params, row->box_y, row->box_height,
                               row->align_bottom, row->bar_x, row->bar_width,
                               &safe_x, &safe_width);
    if (!config->dual_show_bars)
    {
        const double band_height = row->box_height * 0.65;
        const double band_y = row->align_bottom
                                  ? row->box_y + row->box_height - band_height
                                  : row->box_y;
        calculate_safe_region_bounds(params, band_y, band_height, 0.96,
                                     row->bar_x, row->bar_width, &safe_x, &safe_width);
    }

    const double margin = config->layout_label_margin_left > 0
                              ? config->layout_label_margin_left / 100.0
                              : 0.01;
    row->label_x = fmax(safe_x, safe_x + safe_width * margin +
                                get_scaled_label_offset_x(config, params));
    const double gap = get_dual_minimum_spacing(config, params);
    const double label_width = fmax(1.0, safe_x + safe_width - row->label_x - gap - 1.0);
    row->label_size = fit_text_font_size(cr, row->label, preferred_size,
                                        label_width, row->box_height, 1.0);

    cairo_text_extents_t ext = {0};
    cairo_set_font_size(cr, row->label_size);
    cairo_text_extents(cr, row->label, &ext);
    row->value_x = row->label_x + fmax(ext.x_advance, ext.width) + gap;
    row->value_width = fmax(1.0, safe_x + safe_width - row->value_x);
}

/* Keep the preset's font-size ratio when fitting the reading and its label. */
static int draw_dual_value(cairo_t *cr, const Config *config,
                           const monitor_sensor_data_t *data,
                           const ScalingParams *params, DualRow *row)
{
    SlotValueLayout value = {0};
    double label_size = fmax(1.0, row->box_height * 0.5);
    for (int pass = 0; pass < 24; pass++)
    {
        calculate_dual_text_row(cr, config, params, label_size, row);
        layout_and_render_slot_value(cr, data, config, params, row->slot, row->temperature,
                                     row->value_x, row->box_y, row->value_width,
                                     row->box_height, row->align_bottom, 0, &value);
        if (!value.active)
            return 0;

        label_size = value.font_size * row->label_to_value_ratio;
        if (fabs(row->label_size - label_size) < 0.05)
            break;
    }

    layout_and_render_slot_value(cr, data, config, params, row->slot, row->temperature,
                                 row->value_x, row->box_y, row->value_width,
                                 row->box_height, row->align_bottom, 1, &value);
    return 1;
}

static void draw_dual_label(cairo_t *cr, const Config *config,
                           const ScalingParams *params, const DualRow *row)
{
    cairo_text_extents_t ext = {0};
    cairo_set_font_size(cr, row->label_size);
    cairo_text_extents(cr, row->label, &ext);
    const double offset = get_scaled_label_offset_y(config, params);
    double baseline = row->box_y - ext.y_bearing;
    if (row->align_bottom)
    {
        baseline = (row->box_y + row->box_height) - ext.y_bearing - ext.height;
        baseline = fmin(baseline + offset, baseline);
    }
    else
        baseline = fmax(baseline + offset, baseline);
    log_layout_box(cr, config, "label", row->label_x, baseline + ext.y_bearing, ext.width, ext.height);
    cairo_move_to(cr, row->label_x - ext.x_bearing, baseline);
    cairo_show_text(cr, row->label);
}

static void draw_dual_bar(cairo_t *cr, const Config *config,
                         const ScalingParams *params, const DualRow *row)
{
    const int fill_width = calculate_temp_fill_width(row->temperature, row->bar_width, row->max_scale);
    const LayoutBox box = {row->bar_x, row->bar_y, row->bar_width, row->bar_height};
    draw_temperature_bar(cr, config, params, &box, fill_width, &row->bar_color, BAR_ROUNDED_FILL);
}

int paint_dual_overlay(cairo_t *cr, const Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params)
{
    pthread_once(&dual_preset_once, load_dual_preset);
    if (!dual_screen)
        return 0;

    if (config->display_rotation == 0 || params->is_circular)
    {
        if (config->split_ring_by_sensor)
            draw_split_sensor_ring(cr, config, data, params,
                                   config->sensor_slot_1, config->sensor_slot_3);
        else
            draw_sensor_ring(cr, config, data, params);
    }

    DualRow rows[DUAL_ROW_COUNT] = {0};
    if (!calculate_dual_layout(config, data, params, rows))
        return cairo_status(cr) == CAIRO_STATUS_SUCCESS;

    if (config->dual_show_bars)
        for (int i = 0; i < DUAL_ROW_COUNT; i++)
            if (rows[i].active)
                draw_dual_bar(cr, config, params, &rows[i]);

    const char *face = config->font_face[0]
                           ? config->font_face : dual_screen->elements[DUAL_VALUE].font_face;
    cairo_select_font_face(cr, face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    set_cairo_color(cr, &config->font_color_temp);
    for (int i = 0; i < DUAL_ROW_COUNT; i++)
        if (rows[i].active && rows[i].box_height > 0.0 &&
            !draw_dual_value(cr, config, data, params, &rows[i]))
            return cairo_status(cr) == CAIRO_STATUS_SUCCESS;

    cairo_set_font_size(cr, config->font_size_labels);
    set_cairo_color(cr, &config->font_color_label);
    for (int i = 0; i < DUAL_ROW_COUNT; i++)
        if (rows[i].active)
            draw_dual_label(cr, config, params, &rows[i]);
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

/**
 * @brief Display rendering - creates surface, renders content, saves PNG (Dual
 * mode - CPU+GPU).
 */
int render_dual_preview(const struct Config *config,
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

    const FrameRing ring = {
        .first_slot = config->split_ring_by_sensor ? config->sensor_slot_1 : config->sensor_ring_sensor,
        .second_slot = config->sensor_slot_3,
        .split = config->split_ring_by_sensor,
    };
    return render_frame(config, data, &scaling_params, paint_dual_overlay, &ring);
}
