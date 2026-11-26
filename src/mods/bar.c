/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <string.h>

#include "bar.h"

void draw_segment_bar(cairo_t *cr, double x, double y, double width, double height,
                      int segments, double progress, const Color *color)
{
    const double step = width / segments;
    const double gap = segments > 1 ? fmin(step * 0.24, height * 0.4) : 0;
    cairo_save(cr);
    for (int i = 0; i < segments; i++)
        cairo_rectangle(cr, x + step * i, y, step - gap, height);
    cairo_clip(cr);
    set_cairo_color_alpha(cr, color, 0.22);
    cairo_paint(cr);
    if (progress > 0)
    {
        set_cairo_color(cr, color);
        cairo_rectangle(cr, x, y, width * progress, height);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

static void paint_bar_border(cairo_t *cr, const Config *config, const ScalingParams *params,
                             const LayoutBox *box)
{
    if (!config->layout_bar_border_enabled || config->layout_bar_border <= 0.0f)
        return;

    set_cairo_color_alpha(cr, &config->layout_bar_color_border, config->layout_bar_opacity);
    draw_rounded_rectangle_path(cr, box->x, box->y, box->width, box->height, params->corner_radius);
    cairo_set_line_width(cr, get_scaled_bar_border_width(config, params));
    cairo_stroke(cr);
}

void draw_temperature_bar(cairo_t *cr, const Config *config, const ScalingParams *params,
                          const LayoutBox *box, int fill_width, const Color *color, BarStyle style)
{
    const double opacity = config->layout_bar_opacity;
    set_cairo_color_alpha(cr, &config->layout_bar_color_background, opacity);
    draw_rounded_rectangle_path(cr, box->x, box->y, box->width, box->height, params->corner_radius);
    cairo_fill(cr);

    if (style == BAR_CLIPPED_FILL)
        paint_bar_border(cr, config, params, box);

    if (fill_width > 0)
    {
        set_cairo_color_alpha(cr, color, opacity);
        if (style == BAR_CLIPPED_FILL)
        {
            cairo_save(cr);
            draw_rounded_rectangle_path(cr, box->x, box->y, box->width, box->height, params->corner_radius);
            cairo_clip(cr);
            cairo_rectangle(cr, box->x, box->y, fill_width, box->height);
            cairo_fill(cr);
            cairo_restore(cr);
        }
        else
        {
            if (fill_width >= 16)
                draw_rounded_rectangle_path(cr, box->x, box->y, fill_width, box->height, params->corner_radius);
            else
                cairo_rectangle(cr, box->x, box->y, fill_width, box->height);
            cairo_fill(cr);
        }
    }

    if (style == BAR_ROUNDED_FILL)
        paint_bar_border(cr, config, params, box);
}

/**
 * @brief Calculate temperature fill width with bounds checking.
 */
int calculate_temp_fill_width(float temp_value, int max_width, float max_temp)
{
    if (temp_value <= 0.0f)
        return 0;

    const float ratio = fminf(temp_value / max_temp, 1.0f);
    return (int)(ratio * max_width);
}

/**
 * @brief Draw rounded rectangle path for temperature bars.
 */
void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width,
                                 int height, double radius)
{
    if (radius <= 0.0)
    {
        cairo_rectangle(cr, x, y, width, height);
        return;
    }
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -DISPLAY_M_PI_2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0,
              DISPLAY_M_PI_2);
    cairo_arc(cr, x + radius, y + height - radius, radius, DISPLAY_M_PI_2,
              DISPLAY_M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, DISPLAY_M_PI,
              1.5 * DISPLAY_M_PI);
    cairo_close_path(cr);
}

/**
 * @brief Get bar height for a specific slot.
 */
uint16_t get_slot_bar_height(const struct Config *config, const char *slot_name)
{
    if (!config || !slot_name)
        return 20;

    if (strcmp(slot_name, "1") == 0)
    {
        return (config->layout_bar_height_1 > 0)
                   ? config->layout_bar_height_1
                   : config->layout_bar_height;
    }
    else if (strcmp(slot_name, "2") == 0)
    {
        return (config->layout_bar_height_2 > 0)
                   ? config->layout_bar_height_2
                   : config->layout_bar_height;
    }
    else if (strcmp(slot_name, "3") == 0)
    {
        return (config->layout_bar_height_3 > 0)
                   ? config->layout_bar_height_3
                   : config->layout_bar_height;
    }

    return config->layout_bar_height;
}

/**
 * @brief Get the scaled pixel bar height for a slot.
 */
int get_scaled_slot_bar_height(const struct Config *config,
                               const ScalingParams *params,
                               const char *slot_name)
{
    const uint16_t logical_height = get_slot_bar_height(config, slot_name);
    const int scaled_height =
        (int)lround(scale_value_avg(params, (double)logical_height));

    return (scaled_height > 0) ? scaled_height : 1;
}
