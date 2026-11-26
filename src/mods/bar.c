/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <string.h>

#include "display.h"

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
