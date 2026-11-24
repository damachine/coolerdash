/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <time.h>

#include "custom.h"

static void paint_element_text(cairo_t *cr, const Config *config,
                               const CustomElement *element,
                               const sensor_entry_t *sensor, double scale,
                               double width, double height)
{
    char text[CUSTOM_LAYOUT_TEXT_SIZE + 32];
    if (element->type == CUSTOM_VALUE)
    {
        const char *separator = element->text[0] ? "  " : "";
        if (sensor)
            snprintf(text, sizeof(text), "%s%s%.*f %s", element->text, separator,
                     sensor->use_decimal ? 1 : 0, sensor->value, sensor->unit);
        else
            snprintf(text, sizeof(text), "%s%s—", element->text, separator);
    }
    else if (element->type == CUSTOM_CLOCK)
    {
        snprintf(text, sizeof(text), "12:34");
        if (!config->preview_render)
        {
            time_t now = time(NULL);
            struct tm local;
            if (!localtime_r(&now, &local) || !strftime(text, sizeof(text), "%H:%M", &local))
                snprintf(text, sizeof(text), "—");
        }
    }
    else
        snprintf(text, sizeof(text), "%s", element->text);

    cairo_select_font_face(cr, element->monospace ? "monospace" : "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    double size = element->font_size * scale;
    cairo_set_font_size(cr, size);
    cairo_text_extents_t ink;
    cairo_text_extents(cr, text, &ink);
    if (ink.width <= 0 || ink.height <= 0)
        return;
    const double fit = fmin(1.0, fmin(width * 0.96 / ink.width, height * 0.92 / ink.height));
    if (fit < 1.0)
    {
        cairo_set_font_size(cr, size * fit);
        cairo_text_extents(cr, text, &ink);
    }
    double left = -ink.x_bearing;
    if (element->type == CUSTOM_CLOCK || !element->text[0])
        left += (width - ink.width) / 2.0;
    cairo_move_to(cr, left, (height - ink.height) / 2.0 - ink.y_bearing);
    cairo_show_text(cr, text);
}

static void paint_ring(cairo_t *cr, const CustomElement *element, double progress,
                       double width, double height, double scale)
{
    const double diameter = fmin(width, height);
    const double stroke = fmin(diameter * 0.09, 9.0 * scale);
    const double radius = (diameter - stroke) / 2.0;
    const double start = -DISPLAY_M_PI_2;
    cairo_set_line_width(cr, stroke);
    set_cairo_color_alpha(cr, &element->color, 0.22);
    cairo_arc(cr, width / 2.0, height / 2.0, radius, 0, 2.0 * DISPLAY_M_PI);
    cairo_stroke(cr);
    if (progress > 0)
    {
        set_cairo_color(cr, &element->color);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_arc(cr, width / 2.0, height / 2.0, radius, start,
                  start + progress * 2.0 * DISPLAY_M_PI);
        cairo_stroke(cr);
    }
}

int paint_custom_overlay(cairo_t *cr, const Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params)
{
    const double sx = (double)config->display_width / CUSTOM_LAYOUT_GRID;
    const double sy = (double)config->display_height / CUSTOM_LAYOUT_GRID;
    cairo_save(cr);
    if (params->is_circular)
    {
        cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                  params->circle_radius, 0, 2.0 * DISPLAY_M_PI);
        cairo_clip(cr);
    }

    for (int i = 0; i < config->custom_layout.count; i++)
    {
        const CustomElement *element = &config->custom_layout.elements[i];
        const sensor_entry_t *sensor = element->source[0]
                                           ? find_sensor_for_slot(data, element->source)
                                           : NULL;
        if (sensor && !isfinite(sensor->value))
            sensor = NULL;
        const double width = element->width * sx;
        const double height = element->height * sy;
        cairo_save(cr);
        cairo_translate(cr, element->x * sx, element->y * sy);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_clip(cr);
        set_cairo_color(cr, &element->color);

        if (element->type <= CUSTOM_CLOCK)
            paint_element_text(cr, config, element, sensor, params->scale_uniform, width, height);
        else if (element->type == CUSTOM_FRAME)
        {
            const double line = fmin(1.5 * params->scale_uniform, fmin(width, height) / 4.0);
            cairo_set_line_width(cr, line);
            cairo_rectangle(cr, line / 2.0, line / 2.0, width - line, height - line);
            cairo_stroke(cr);
        }
        else
        {
            const double progress = sensor ? fmax(0, fmin(1, (sensor->value - element->minimum) /
                                              (element->maximum - element->minimum))) : 0;
            if (element->type == CUSTOM_BAR)
                draw_segment_bar(cr, 0, 0, width, height, element->segments,
                                 progress, &element->color);
            else
                paint_ring(cr, element, progress, width, height, params->scale_uniform);
        }
        cairo_restore(cr);
    }
    cairo_restore(cr);
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}
