/* SPDX-License-Identifier: MIT */
#include <math.h>

#include "display.h"

static double get_sensor_ring_line_width(const struct Config *config,
                                         const ScalingParams *params,
                                         double reference_radius)
{
    if (config->layout_ring_width > 0.0f)
        return fmax(1.0, scale_value_avg(params, config->layout_ring_width));
    return fmax(2.0, reference_radius * 0.035);
}

void draw_sensor_ring(cairo_t *cr, const struct Config *config,
                      const monitor_sensor_data_t *data,
                      const ScalingParams *params)
{
    if (!config)
        return;

    draw_sensor_ring_for_slot(cr, config, data, params,
                              config->sensor_ring_sensor);
}

void draw_sensor_ring_for_slot(cairo_t *cr, const struct Config *config,
                               const monitor_sensor_data_t *data,
                               const ScalingParams *params,
                               const char *slot_value)
{
    if (!cr || !config || !data || !params ||
        !config->sensor_ring_enabled ||
        !slot_is_active(slot_value))
        return;

    const sensor_entry_t *sensor = find_sensor_for_slot(data, slot_value);
    if (!sensor)
        return;

    const double max_value = fmax(
        1.0, get_slot_max_scale(config, slot_value));
    const double progress = fmax(0.0, fmin(1.0, sensor->value / max_value));
    const double min_dimension = fmin((double)config->display_width,
                                      (double)config->display_height);
    const double content_scale =
        (config->display_content_scale_factor > 0.0f &&
         config->display_content_scale_factor <= 1.0f)
            ? config->display_content_scale_factor
            : 0.98;
    const double reference_radius = params->is_circular
                                        ? params->circle_radius
                                        : min_dimension * 0.5 * content_scale;
    const double line_width =
        get_sensor_ring_line_width(config, params, reference_radius);
    const double opacity = config->layout_ring_opacity;
    const Color active = get_slot_bar_color(config, slot_value, sensor->value);

    cairo_save(cr);
    cairo_set_line_width(cr, line_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (!params->is_circular)
    {
        const double padding_x =
            ((double)config->display_width * (1.0 - content_scale) * 0.5) +
            line_width * 0.75;
        const double padding_y =
            ((double)config->display_height * (1.0 - content_scale) * 0.5) +
            line_width * 0.75;
        const int x = (int)lround(padding_x);
        const int y = (int)lround(padding_y);
        const int width = config->display_width - 2 * x;
        const int height = config->display_height - 2 * y;
        const double short_side = fmin((double)width, (double)height);
        const double corner_radius = config->layout_ring_rounded_corners
            ? fmin(short_side * 0.5,
                   fmax(line_width, short_side *
                       (params->shape == DISPLAY_SHAPE_ROUNDED_SQUARE ? 0.16 : 0.08)))
            : 0.0;

        if (width > 2 && height > 2)
        {
            const double perimeter =
                2.0 * ((double)width + (double)height -
                       4.0 * corner_radius) +
                2.0 * DISPLAY_M_PI * corner_radius;

            set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                                  0.72 * opacity);
            draw_rounded_rectangle_path(cr, x, y, width, height,
                                        corner_radius);
            cairo_stroke(cr);

            if (progress > 0.0)
            {
                set_cairo_color_alpha(cr, &active, opacity);
                if (progress < 1.0 && perimeter > 0.0)
                {
                    const double dashes[2] = {
                        perimeter * progress,
                        perimeter * (1.0 - progress)};
                    /* Move the value arc, not the rectangular outline. */
                    const double offset =
                        -perimeter * config->display_rotation / 360.0;
                    cairo_set_dash(cr, dashes, 2, offset);
                }
                draw_rounded_rectangle_path(cr, x, y, width, height,
                                            corner_radius);
                cairo_stroke(cr);
            }
        }
        cairo_restore(cr);
        return;
    }

    const double radius = fmax(1.0, params->circle_radius - line_width * 0.75);
    const double degrees = DISPLAY_M_PI / 180.0;
    const double start = 135.0 * degrees;
    const double end = start + 270.0 * degrees;
    set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                          0.72 * opacity);
    cairo_new_sub_path(cr);
    cairo_arc(cr, params->circle_center_x, params->circle_center_y,
              radius, start, end);
    cairo_stroke(cr);

    if (progress > 0.0)
    {
        set_cairo_color_alpha(cr, &active, opacity);
        cairo_new_sub_path(cr);
        cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                  radius, start, start + (end - start) * progress);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void draw_split_sensor_ring(cairo_t *cr, const struct Config *config,
                            const monitor_sensor_data_t *data,
                            const ScalingParams *params,
                            const char *left_slot, const char *right_slot)
{
    if (!cr || !config || !data || !params ||
        !config->split_ring_by_sensor)
        return;

    const sensor_entry_t *left = find_sensor_for_slot(data, left_slot);
    const sensor_entry_t *right = find_sensor_for_slot(data, right_slot);
    if (!left && !right)
        return;

    const double min_dimension = fmin((double)config->display_width,
                                      (double)config->display_height);
    const double content_scale =
        (config->display_content_scale_factor > 0.0f &&
         config->display_content_scale_factor <= 1.0f)
            ? config->display_content_scale_factor
            : 0.98;
    const double reference_radius = params->is_circular
                                        ? params->circle_radius
                                        : min_dimension * 0.5 * content_scale;
    const double line_width =
        get_sensor_ring_line_width(config, params, reference_radius);
    const double opacity = config->layout_ring_opacity;

    cairo_save(cr);
    cairo_set_line_width(cr, line_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (params->is_circular)
    {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        const double radius =
            fmax(1.0, params->circle_radius - line_width * 0.75);
        const double start = 135.0 * DISPLAY_M_PI / 180.0;
        const double middle = start + 135.0 * DISPLAY_M_PI / 180.0;
        const double end = start + 270.0 * DISPLAY_M_PI / 180.0;

        set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                              0.72 * opacity);
        cairo_new_sub_path(cr);
        cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                  radius, start, end);
        cairo_stroke(cr);

        if (left)
        {
            const Color color = get_slot_bar_color(config, left_slot,
                                                   left->value);
            set_cairo_color_alpha(cr, &color, opacity);
            cairo_new_sub_path(cr);
            cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                      radius, start, middle);
            cairo_stroke(cr);
        }
        if (right)
        {
            const Color color = get_slot_bar_color(config, right_slot,
                                                   right->value);
            set_cairo_color_alpha(cr, &color, opacity);
            cairo_new_sub_path(cr);
            cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                      radius, middle, end);
            cairo_stroke(cr);
        }
        cairo_restore(cr);
        return;
    }

    const double padding_x =
        ((double)config->display_width * (1.0 - content_scale) * 0.5) +
        line_width * 0.75;
    const double padding_y =
        ((double)config->display_height * (1.0 - content_scale) * 0.5) +
        line_width * 0.75;
    const int x = (int)lround(padding_x);
    const int y = (int)lround(padding_y);
    const int width = config->display_width - 2 * x;
    const int height = config->display_height - 2 * y;
    if (width <= 2 || height <= 2)
    {
        cairo_restore(cr);
        return;
    }

    const double short_side = fmin((double)width, (double)height);
    const double corner_radius = config->layout_ring_rounded_corners
        ? fmin(short_side * 0.5,
               fmax(line_width, short_side *
                   (params->shape == DISPLAY_SHAPE_ROUNDED_SQUARE ? 0.16 : 0.08)))
        : 0.0;

    set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                          0.72 * opacity);
    draw_rounded_rectangle_path(cr, x, y, width, height, corner_radius);
    cairo_stroke(cr);

    const sensor_entry_t *entries[2] = {left, right};
    const char *slots[2] = {left_slot, right_slot};
    const double perimeter =
        2.0 * ((double)width + (double)height - 4.0 * corner_radius) +
        2.0 * DISPLAY_M_PI * corner_radius;
    const double half = perimeter * 0.5;
    /* The path starts at the top right. The left half starts at bottom center. */
    const double left_start = half - width * 0.5 + corner_radius;
    const double shift = left_start +
                         perimeter * config->display_rotation / 360.0;
    for (int i = 0; i < 2; i++)
    {
        if (!entries[i])
            continue;
        const Color color =
            get_slot_bar_color(config, slots[i], entries[i]->value);
        cairo_save(cr);
        if (config->display_rotation == 0)
        {
            cairo_rectangle(cr, i == 0 ? 0.0 : config->display_width * 0.5,
                            0.0, config->display_width * 0.5,
                            config->display_height);
            cairo_clip(cr);
        }
        else
        {
            const double dashes[2] = {half, half};
            cairo_set_dash(cr, dashes, 2, i == 0 ? -shift : half - shift);
        }
        set_cairo_color_alpha(cr, &color, opacity);
        draw_rounded_rectangle_path(cr, x, y, width, height, corner_radius);
        cairo_stroke(cr);
        cairo_restore(cr);
    }
    cairo_restore(cr);
}
