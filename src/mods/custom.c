/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "custom.h"
#include "tty.h"

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

static void add_ring_arcs(cairo_t *cr, double x, double y, double radius,
                          double start, double sweep, int segments, double progress)
{
    const double step = sweep / segments;
    const double gap = segments > 1 ? step * 0.22 : 0;
    const double limit = sweep * progress;
    for (int i = 0; i < segments && i * step < limit; i++)
    {
        const double offset = i * step;
        cairo_new_sub_path(cr);
        cairo_arc(cr, x, y, radius, start + offset, start + fmin(offset + step - gap, limit));
    }
}

static void paint_ring(cairo_t *cr, const CustomElement *element, double progress,
                       double width, double height, double scale)
{
    const double diameter = fmin(width, height);
    const double stroke = element->ring_thickness > 0
                              ? fmin(diameter / 2, element->ring_thickness * scale)
                              : fmin(diameter * 0.09, 9.0 * scale);
    const double radius = (diameter - stroke) / 2.0;
    const int degrees = element->ring_sweep > 0 ? element->ring_sweep : 360;
    const int segments = element->segments > 0 ? element->segments : 1;
    const double start = (element->ring_start - 90) * DISPLAY_M_PI / 180.0;
    const double sweep = degrees * DISPLAY_M_PI / 180.0;
    const double x = width / 2.0;
    const double y = height / 2.0;
    cairo_set_line_width(cr, stroke);
    if (segments == 1 && degrees < 360)
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    set_cairo_color_alpha(cr, &element->color, 0.22);
    add_ring_arcs(cr, x, y, radius, segments == 1 && degrees == 360 ? 0 : start,
                  sweep, segments, 1);
    cairo_stroke(cr);
    if (progress > 0)
    {
        set_cairo_color(cr, &element->color);
        if (segments == 1)
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        add_ring_arcs(cr, x, y, radius, start, sweep, segments, progress);
        cairo_stroke(cr);
    }
}

typedef struct
{
    sensor_entry_t sensor;
    double gib, memory_load;
    int memory_status;
} WidgetReadings;

static const sensor_entry_t *widget_sensor(const Config *config, const monitor_sensor_data_t *data,
                                           const char *id, WidgetReadings *readings)
{
    const CustomSource source = get_custom_source(id);
    if (source == CUSTOM_SOURCE_SENSOR)
        return find_sensor_for_slot(data, id);
    if (source == CUSTOM_SOURCE_CPU_LOAD || source == CUSTOM_SOURCE_GPU_LOAD)
        return find_processor_load(data, source == CUSTOM_SOURCE_CPU_LOAD ? "cpu" : "gpu");
    if (source == CUSTOM_SOURCE_CPU_POWER || source == CUSTOM_SOURCE_GPU_POWER)
        return find_channel_sensor_for_slot(data, source == CUSTOM_SOURCE_CPU_POWER ? "cpu" : "gpu",
                                            SENSOR_CATEGORY_WATTS);

    double value;
    const char *unit;
    if (source == CUSTOM_SOURCE_POWER)
    {
        if (!read_processor_power(data, &value, NULL))
            return NULL;
        unit = "W";
    }
    else
    {
        if (!readings->memory_status)
            readings->memory_status =
                read_used_memory(config, &readings->gib, &readings->memory_load) ? 1 : -1;
        if (readings->memory_status < 0)
            return NULL;
        value = source == CUSTOM_SOURCE_RAM_USED ? readings->gib : readings->memory_load * 100;
        unit = source == CUSTOM_SOURCE_RAM_USED ? "GiB" : "%";
    }
    readings->sensor.value = (float)value;
    readings->sensor.use_decimal = source == CUSTOM_SOURCE_RAM_USED;
    snprintf(readings->sensor.unit, sizeof(readings->sensor.unit), "%s", unit);
    return &readings->sensor;
}

static int paint_widget_layout(cairo_t *cr, const Config *config,
                               const monitor_sensor_data_t *data,
                               const ScalingParams *params,
                               const CustomScreen *screen)
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

    WidgetReadings readings = {0};
    for (int i = 0; i < screen->count; i++)
    {
        const CustomElement *element = &screen->elements[i];
        const sensor_entry_t *sensor = element->source[0]
                                           ? widget_sensor(config, data, element->source, &readings)
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

static int active_screen = 0;
static int64_t screen_started_us = -1;

void reset_custom_state(void)
{
    active_screen = 0;
    screen_started_us = -1;
}

int64_t custom_screen_deadline_us(const Config *config)
{
    const CustomLayout *layout = &config->custom_layout;
    if (layout->screen_count <= 1 || active_screen >= layout->screen_count ||
        screen_started_us < 0 || config->preview_render || strcmp(config->display_mode, "custom") != 0)
        return 0;
    return screen_started_us + (int64_t)layout->screens[active_screen].duration * 1000000;
}

int paint_custom_overlay(cairo_t *cr, const Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params)
{
    const CustomLayout *layout = &config->custom_layout;
    if (config->preview_render || layout->screen_count <= 1)
        return paint_widget_layout(cr, config, data, params, &layout->screens[0]);

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
    {
        const int64_t now_us = (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
        if (screen_started_us < 0 || active_screen >= layout->screen_count)
        {
            active_screen = 0;
            screen_started_us = now_us;
        }
        else if (now_us >= custom_screen_deadline_us(config))
        {
            active_screen = (active_screen + 1) % layout->screen_count;
            screen_started_us = now_us;
        }
    }
    return paint_widget_layout(cr, config, data, params, &layout->screens[active_screen]);
}

int paint_activity_rings_overlay(cairo_t *cr, const Config *config,
                                 const monitor_sensor_data_t *data,
                                 const ScalingParams *params)
{
    static const CustomScreen screen = {
        .count = 5,
        .elements = {
            {.type = CUSTOM_RING, .x = 55, .y = 55, .width = 890, .height = 890,
             .maximum = 100, .color = {255, 102, 133, 1}, .source = "cpu"},
            {.type = CUSTOM_RING, .x = 120, .y = 120, .width = 760, .height = 760,
             .maximum = 100, .color = {180, 235, 121, 1}, .source = "gpu"},
            {.type = CUSTOM_RING, .x = 185, .y = 185, .width = 630, .height = 630,
             .maximum = 100, .color = {92, 222, 245, 1}, .source = "liquid"},
            {.type = CUSTOM_VALUE, .x = 260, .y = 375, .width = 480, .height = 190,
             .font_size = 34, .color = {233, 241, 255, 1}, .source = "cpu"},
            {.type = CUSTOM_TEXT, .x = 427, .y = 566, .width = 146, .height = 94,
             .font_size = 15, .color = {255, 141, 162, 1}, .text = "CPU"},
        },
    };

    cairo_save(cr);
    if (config->display_show_clock)
    {
        cairo_translate(cr, config->display_width * 0.075, config->display_height * 0.025);
        cairo_scale(cr, 0.85, 0.85);
    }
    const int painted = paint_widget_layout(cr, config, data, params, &screen);
    cairo_restore(cr);
    return painted;
}
