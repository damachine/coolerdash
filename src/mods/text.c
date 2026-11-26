/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "text.h"

static double clamp_double(double value, double min_value, double max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

double fit_text_font_size(cairo_t *cr, const char *text,
                          double preferred_size, double max_width,
                          double max_height, double minimum_size)
{
    if (!cr || !text || max_width <= 0.0 || max_height <= 0.0)
        return fmax(1.0, minimum_size);

    double size = fmax(1.0, preferred_size);
    const double floor_size = fmax(1.0, minimum_size);

    while (size > floor_size)
    {
        cairo_text_extents_t text_ext = {0};
        cairo_set_font_size(cr, size);
        cairo_text_extents(cr, text, &text_ext);

        if (fmax(text_ext.x_advance, text_ext.width) <= max_width &&
            text_ext.height <= max_height)
            return size;

        size *= 0.92;
    }

    /* The lower bound is soft: geometry must remain valid on tiny displays. */
    size = floor_size;
    while (size > 1.0)
    {
        cairo_text_extents_t text_ext = {0};
        cairo_set_font_size(cr, size);
        cairo_text_extents(cr, text, &text_ext);
        if (fmax(text_ext.x_advance, text_ext.width) <= max_width &&
            text_ext.height <= max_height)
            break;
        size *= 0.92;
    }
    return fmax(1.0, size);
}

/**
 * @brief Get effective degree symbol spacing for the current display.
 */
static int get_scaled_degree_spacing(const struct Config *config,
                              const ScalingParams *params)
{
    const double base_spacing =
        (config && config->display_degree_spacing > 0)
            ? (double)config->display_degree_spacing
            : 16.0;

    const int scaled_spacing = (int)lround(scale_value_avg(params, base_spacing));
    return (scaled_spacing > 0) ? scaled_spacing : 1;
}

/**
 * @brief Format a sensor value with optional decimals.
 */
void format_slot_value_text(char *buffer, size_t buffer_size,
                            const monitor_sensor_data_t *data,
                            const char *slot_value, float temp_value)
{
    if (!buffer || buffer_size == 0)
        return;

    if (get_slot_use_decimal(data, slot_value))
        snprintf(buffer, buffer_size, "%.1f", temp_value);
    else
        snprintf(buffer, buffer_size, "%d", (int)temp_value);
}

typedef struct
{
    cairo_text_extents_t number;
    cairo_text_extents_t degree;
    double number_width;
    double width;
    double height;
} SlotValueMetrics;

static SlotValueMetrics measure_slot_value(cairo_t *cr, const char *value_text,
                                           double font_size, int is_temp,
                                           int degree_spacing)
{
    SlotValueMetrics metrics = {0};
    cairo_set_font_size(cr, font_size);
    cairo_text_extents(cr, value_text, &metrics.number);
    metrics.number_width = fmax(metrics.number.x_advance, metrics.number.width);
    metrics.width = metrics.number_width;
    metrics.height = metrics.number.height;
    if (is_temp)
    {
        cairo_set_font_size(cr, font_size / 2.05);
        cairo_text_extents(cr, "\xC2\xB0", &metrics.degree);
        cairo_set_font_size(cr, font_size);
        metrics.width += degree_spacing +
                         fmax(metrics.degree.x_advance, metrics.degree.width);
        metrics.height = fmax(metrics.height,
                              metrics.number.height * 0.08 + metrics.degree.height);
    }
    return metrics;
}

/**
 * @brief Measure and optionally render a right-aligned sensor value block.
 */
void layout_and_render_slot_value(cairo_t *cr,
                                  const monitor_sensor_data_t *data,
                                  const struct Config *config,
                                  const ScalingParams *params,
                                  const char *slot_value,
                                  float temp_value, double box_x,
                                  double box_y, double box_width,
                                  double box_height, int align_bottom,
                                  int draw_output,
                                  SlotValueLayout *layout)
{
    if (!cr || !data || !config || !params || !slot_value || !layout ||
        box_width <= 0.0 || box_height <= 0.0)
        return;

    memset(layout, 0, sizeof(*layout));

    char value_text[16] = {0};
    format_slot_value_text(value_text, sizeof(value_text), data, slot_value,
                           temp_value);

    const int is_temp = get_slot_is_temp(data, slot_value);
    const double offset_x =
        scale_value_x(params, (double)get_slot_offset_x(config, slot_value));
    const double offset_y =
        scale_value_y(params, (double)get_slot_offset_y(config, slot_value));
    const int scaled_spacing = get_scaled_degree_spacing(config, params);
    const int degree_spacing =
        scaled_spacing > 1 ? (int)lround(scaled_spacing * 0.65) : 1;
    const double temp_margin_factor =
        (config->layout_label_margin_left > 0)
            ? (config->layout_label_margin_left / 200.0)
            : 0.005;
    const double right_margin = fmax(0.0, box_width * temp_margin_factor);
    const double inner_padding_x = clamp_double(scale_value_avg(params, 0.75),
                                                0.0, box_width * 0.015);
    const double inner_padding_y = clamp_double(scale_value_avg(params, 0.25),
                                                0.0, box_height * 0.02);
    const double available_width =
        fmax(1.0, box_width - inner_padding_x - right_margin);
    const double available_height =
        fmax(1.0, box_height - inner_padding_y);
    const double configured_font_size = get_slot_font_size(config, slot_value);
    const double configured_scaled_font_size =
        (configured_font_size > 0.0)
            ? scale_value_avg(params, configured_font_size)
            : 0.0;
    const double auto_font_size = fmax(scale_value_avg(params, 220.0),
                                       available_height * 7.40);
    const double preferred_font_size =
        (configured_scaled_font_size > 0.0)
            ? configured_scaled_font_size
            : auto_font_size;
    double font_size = preferred_font_size;
    SlotValueMetrics metrics;

    for (;;)
    {
        metrics = measure_slot_value(cr, value_text, font_size, is_temp,
                                     degree_spacing);
        if ((metrics.width <= available_width &&
             metrics.height <= available_height) || font_size <= 1.0)
            break;
        font_size = fmax(1.0, font_size * 0.93);
    }

    if (configured_scaled_font_size <= 0.0)
    {
        double growth_step = fmax(0.5, scale_value_avg(params, 1.0));
        while (growth_step >= 0.25)
        {
            for (;;)
            {
                const double candidate_size = font_size + growth_step;
                SlotValueMetrics candidate = measure_slot_value(
                    cr, value_text, candidate_size, is_temp, degree_spacing);
                if (candidate.width > available_width ||
                    candidate.height > available_height)
                    break;
                font_size = candidate_size;
                metrics = candidate;
            }
            growth_step *= 0.5;
        }
        cairo_set_font_size(cr, font_size);
    }

    const double requested_right =
        box_x + box_width - right_margin + offset_x;
    const double minimum_left = box_x + inner_padding_x;
    const double maximum_left =
        fmax(minimum_left,
             box_x + box_width - right_margin - metrics.width);
    const double block_left =
        clamp_double(requested_right - metrics.width,
                     minimum_left, maximum_left);

    double block_top;
    if (align_bottom)
    {
        const double ink_bottom =
            box_y + box_height - inner_padding_y + offset_y;
        block_top = ink_bottom - metrics.height;
    }
    else
    {
        block_top = box_y + inner_padding_y + offset_y;
    }

    const double minimum_top = box_y + inner_padding_y;
    const double maximum_top =
        fmax(minimum_top,
             box_y + box_height - inner_padding_y - metrics.height);
    block_top = clamp_double(block_top, minimum_top, maximum_top);

    const double local_baseline = -metrics.number.y_bearing;
    const double baseline_y = block_top + local_baseline;

    if (draw_output)
    {
        log_layout_box(cr, config, "value", block_left, block_top, metrics.width, metrics.height);
        cairo_move_to(cr, block_left - metrics.number.x_bearing, baseline_y);
        cairo_show_text(cr, value_text);

        if (is_temp)
        {
            const double degree_top = block_top + metrics.number.height * 0.08;
            const double degree_x =
                block_left + metrics.number_width + degree_spacing -
                metrics.degree.x_bearing;
            const double degree_y = degree_top - metrics.degree.y_bearing;

            cairo_set_font_size(cr, font_size / 2.05);
            cairo_move_to(cr, degree_x, degree_y);
            cairo_show_text(cr, "\xC2\xB0");
            cairo_set_font_size(cr, font_size);
        }
    }

    layout->active = 1;
    layout->block_left = block_left;
    layout->block_right = block_left + metrics.width;
    layout->block_top = block_top;
    layout->block_bottom = block_top + metrics.height;
    layout->baseline_y = baseline_y;
    layout->font_size = font_size;
}

void render_centered_text(cairo_t *cr, const char *text,
                               double center_x, double center_y,
                               double preferred_size, double max_width,
                               double max_height, const Color *color)
{
    if (!cr || !text || text[0] == '\0' || !color || max_width <= 0.0)
        return;

    const double font_size = fit_text_font_size(
        cr, text, preferred_size, max_width, max_height, 6.0);
    cairo_text_extents_t ext = {0};
    cairo_set_font_size(cr, font_size);
    cairo_text_extents(cr, text, &ext);
    set_cairo_color(cr, color);
    cairo_move_to(cr, center_x - (ext.width / 2.0 + ext.x_bearing),
                  center_y - (ext.height / 2.0 + ext.y_bearing));
    cairo_show_text(cr, text);
}

void render_centered_value_with_small_unit(
    cairo_t *cr, const char *text, double center_x, double center_y,
    double preferred_size, double max_width, double max_height,
    const Color *color, const ScalingParams *params)
{
    if (!cr || !text || text[0] == '\0' || !color || !params ||
        max_width <= 0.0 || max_height <= 0.0)
        return;

    const size_t text_len = strlen(text);
    const char unit = text[text_len - 1];
    if (unit != '%' && unit != 'W')
    {
        render_centered_text(cr, text, center_x, center_y, preferred_size,
                           max_width, max_height, color);
        return;
    }

    char number[24] = {0};
    size_t number_len = text_len - 1;
    while (number_len > 0 && text[number_len - 1] == ' ')
        number_len--;
    if (number_len >= sizeof(number))
        number_len = sizeof(number) - 1;
    memcpy(number, text, number_len);
    number[number_len] = '\0';

    double number_size = fmax(1.0, preferred_size);
    double unit_size;
    const double unit_gap = fmax(1.0, scale_value_avg(params, 1.0));
    cairo_text_extents_t number_ext = {0};
    cairo_text_extents_t unit_ext = {0};
    double number_width = 0.0;
    double total_width = 0.0;
    double content_height = 0.0;
    char unit_text[2] = {unit, '\0'};

    for (;;)
    {
        cairo_set_font_size(cr, number_size);
        cairo_text_extents(cr, number, &number_ext);
        unit_size = number_size * 0.5;
        cairo_set_font_size(cr, unit_size);
        cairo_text_extents(cr, unit_text, &unit_ext);

        number_width = fmax(number_ext.x_advance, number_ext.width);
        total_width = number_width + unit_gap +
                      fmax(unit_ext.x_advance, unit_ext.width);
        content_height =
            fmax(number_ext.height,
                 number_ext.height * 0.08 + unit_ext.height);
        if ((total_width <= max_width && content_height <= max_height) ||
            number_size <= 1.0)
            break;
        number_size = fmax(1.0, number_size * 0.92);
    }

    const double block_left = center_x - total_width * 0.5;
    const double block_top = center_y - content_height * 0.5;
    const double number_baseline = block_top - number_ext.y_bearing;
    const double unit_top = block_top + number_ext.height * 0.08;

    set_cairo_color(cr, color);
    cairo_set_font_size(cr, number_size);
    cairo_move_to(cr, block_left - number_ext.x_bearing, number_baseline);
    cairo_show_text(cr, number);

    cairo_set_font_size(cr, unit_size);
    cairo_move_to(cr, block_left + number_width + unit_gap - unit_ext.x_bearing,
                  unit_top - unit_ext.y_bearing);
    cairo_show_text(cr, unit_text);
}

double render_centered_temperature(cairo_t *cr, const char *number,
                                        double center_x, double center_y,
                                        double preferred_size, double max_width,
                                        double max_height, const Color *color,
                                        const ScalingParams *params)
{
    if (!cr || !number || number[0] == '\0' || !color || !params ||
        max_width <= 0.0 || max_height <= 0.0)
        return 0.0;

    double number_size = fmax(6.0, preferred_size);
    double degree_size;
    const double degree_gap = fmax(0.5, scale_value_avg(params, 0.5));
    cairo_text_extents_t number_ext = {0};
    cairo_text_extents_t degree_ext = {0};
    cairo_font_extents_t number_font_ext = {0};
    double total_width = 0.0;

    for (;;)
    {
        cairo_set_font_size(cr, number_size);
        cairo_text_extents(cr, number, &number_ext);
        cairo_font_extents(cr, &number_font_ext);

        degree_size = number_size * 0.50;
        cairo_set_font_size(cr, degree_size);
        cairo_text_extents(cr, "\xC2\xB0", &degree_ext);

        total_width = fmax(number_ext.x_advance, number_ext.width) +
                      degree_gap +
                      fmax(degree_ext.x_advance, degree_ext.width);
        if ((total_width <= max_width &&
             number_font_ext.ascent + number_font_ext.descent <= max_height) ||
            number_size <= 1.0)
            break;

        number_size *= 0.92;
    }

    const double number_width = fmax(number_ext.x_advance, number_ext.width);
    const double block_left = center_x - total_width / 2.0;
    const double number_baseline =
        center_y - (number_ext.height / 2.0 + number_ext.y_bearing);

    set_cairo_color(cr, color);
    cairo_set_font_size(cr, number_size);
    cairo_move_to(cr, block_left - number_ext.x_bearing, number_baseline);
    cairo_show_text(cr, number);

    cairo_set_font_size(cr, degree_size);
    cairo_move_to(cr, block_left + number_width + degree_gap -
                          degree_ext.x_bearing,
                  number_baseline + number_ext.y_bearing +
                      number_ext.height * 0.08 - degree_ext.y_bearing);
    cairo_show_text(cr, "\xC2\xB0");
    return number_size;
}
