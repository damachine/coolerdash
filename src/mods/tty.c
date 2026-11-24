/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tty.h"

static const Color terminal_accent = {149, 185, 251, 1};
static const double label_scale = 0.68;

typedef struct
{
    double left;
    double value_right;
    double bar_left;
    double bar_width;
    double baseline;
    double font_size;
} TerminalRow;

typedef struct
{
    cairo_text_extents_t label;
    cairo_text_extents_t value;
    cairo_text_extents_t unit;
    double unit_width;
} TerminalText;

static void draw_centered(cairo_t *cr, double x, double baseline, const char *text)
{
    cairo_text_extents_t ink;
    cairo_text_extents(cr, text, &ink);
    cairo_move_to(cr, x - ink.x_bearing - ink.width / 2, baseline);
    cairo_show_text(cr, text);
}

static double measure_row_text(cairo_t *cr, TerminalText *text, double size, double unit_size,
                              const char *label, const char *value, const char *unit)
{
    cairo_set_font_size(cr, size * label_scale);
    cairo_text_extents(cr, label, &text->label);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, value, &text->value);
    cairo_set_font_size(cr, unit_size);
    cairo_text_extents(cr, unit, &text->unit);
    text->unit_width = *unit ? text->unit.width + size * 0.12 : 0;
    return text->label.width + text->value.width + text->unit_width + size * 0.34;
}

static void draw_row(cairo_t *cr, const Config *config, const TerminalRow *row, const char *label,
                     const char *value, const char *unit, int has_bar, double progress)
{
    TerminalText text;
    const int temperature = strcmp(unit, "°C") == 0;
    double font_size = row->font_size;
    double unit_size = font_size * 0.64;
    const double available = row->value_right - row->left;
    const double span = measure_row_text(cr, &text, font_size, unit_size, label, value, unit);
    if (span > available)
    {
        const double fixed_width = temperature ? text.unit.width : 0;
        font_size = fmax(1, floor(font_size * (available - fixed_width) / (span - fixed_width)));
        if (!temperature)
            unit_size = font_size * 0.64;
        measure_row_text(cr, &text, font_size, unit_size, label, value, unit);
    }

    set_cairo_color(cr, &config->font_color_label);
    cairo_set_font_size(cr, font_size * label_scale);
    cairo_move_to(cr, row->left - text.label.x_bearing, row->baseline);
    cairo_show_text(cr, label);

    set_cairo_color(cr, &config->font_color_temp);
    cairo_set_font_size(cr, font_size);
    cairo_move_to(cr, row->value_right - text.unit_width - text.value.x_bearing - text.value.width,
                  row->baseline);
    cairo_show_text(cr, value);
    if (*unit)
    {
        double unit_baseline = row->baseline;
        if (temperature)
            unit_baseline += text.value.y_bearing - text.unit.y_bearing;
        cairo_set_font_size(cr, unit_size);
        cairo_move_to(cr, row->value_right - text.unit.x_bearing - text.unit.width, unit_baseline);
        cairo_show_text(cr, unit);
    }

    if (has_bar)
    {
        const double bar_height = fmin(row->font_size * 0.35, row->bar_width / 3.0);
        draw_segment_bar(cr, row->bar_left, row->baseline - bar_height * 1.18,
                         row->bar_width, bar_height, 8,
                         fmax(0, fmin(1, progress)), &terminal_accent);
    }
}

static int valid_reading(const sensor_entry_t *sensor)
{
    return sensor && isfinite(sensor->value);
}

static void draw_temperature(cairo_t *cr, const Config *config, const monitor_sensor_data_t *data,
                             const TerminalRow *row, const char *label, const char *slot,
                             int has_bar)
{
    const sensor_entry_t *sensor = find_sensor_for_slot(data, slot);
    const sensor_entry_t *load = has_bar ? find_processor_load(data, slot) : NULL;
    char value[24] = "—";
    const int available = valid_reading(sensor);
    if (available)
        snprintf(value, sizeof(value), "%.*f", sensor->use_decimal ? 1 : 0, sensor->value);
    draw_row(cr, config, row, label, value, available ? "°C" : "", has_bar,
             valid_reading(load) ? load->value / 100.0 : 0);
}

int read_processor_power(const monitor_sensor_data_t *data, double *watts, const char **label)
{
    const sensor_entry_t *cpu = find_channel_sensor_for_slot(data, "cpu", SENSOR_CATEGORY_WATTS);
    const sensor_entry_t *gpu = find_channel_sensor_for_slot(data, "gpu", SENSOR_CATEGORY_WATTS);
    const int has_cpu = valid_reading(cpu);
    const int has_gpu = valid_reading(gpu);
    if (label)
        *label = has_cpu == has_gpu ? "PWR" : has_cpu ? "CPU W" : "GPU W";
    *watts = (has_cpu ? (double)cpu->value : 0) + (has_gpu ? (double)gpu->value : 0);
    return has_cpu || has_gpu;
}

static void draw_power(cairo_t *cr, const Config *config, const monitor_sensor_data_t *data,
                       const TerminalRow *row)
{
    const char *label;
    double watts;
    char value[24] = "—";
    const int available = read_processor_power(data, &watts, &label);
    if (available)
        snprintf(value, sizeof(value), "%.0f", watts);
    draw_row(cr, config, row, label, value, available ? "W" : "", 0, 0);
}

int read_used_memory(const Config *config, double *gib, double *progress)
{
    if (config->preview_render)
    {
        *gib = 18.6;
        *progress = 0.58;
        return 1;
    }
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file)
        return 0;

    unsigned long long total = 0;
    unsigned long long available = 0;
    int has_available = 0;
    char line[128];
    while ((!total || !has_available) && fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "MemAvailable: %llu kB", &available) == 1)
            has_available = 1;
        else
            sscanf(line, "MemTotal: %llu kB", &total);
    }
    fclose(file);
    if (!total || !has_available || available > total)
        return 0;

    const unsigned long long used = total - available;
    *gib = used / 1048576.0;
    *progress = (double)used / total;
    return 1;
}

static void draw_memory(cairo_t *cr, const Config *config, const TerminalRow *row)
{
    double gib;
    double progress;
    char value[24] = "—";
    const int available = read_used_memory(config, &gib, &progress);
    if (available)
        snprintf(value, sizeof(value), "%.1f", gib);
    else
        progress = 0;
    draw_row(cr, config, row, "RAM", value, available ? "GiB" : "", 1, progress);
}

static void draw_prompt(cairo_t *cr, const Config *config, const ScalingParams *params,
                        double left, double top, double height, double scale)
{
    if (config->tty_icon == TTY_ICON_OFF)
        return;

    const int large = config->tty_icon == TTY_ICON_LARGE;
    const double center_x = params->circle_center_x;
    const double tile_y = top + height * 0.175;
    if (large)
    {
        set_cairo_color_alpha(cr, &terminal_accent, 0.2);
        cairo_rectangle(cr, center_x - 24 * scale, tile_y - 16 * scale,
                        48 * scale, 32 * scale);
        cairo_fill(cr);
    }

    set_cairo_color(cr, &config->font_color_label);
    cairo_select_font_face(cr, config->tty_font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, (large ? 23 : 16) * scale);
    if (large)
    {
        draw_centered(cr, center_x, tile_y + 6 * scale, ">_");
        return;
    }

    cairo_text_extents_t ink;
    cairo_text_extents(cr, ">_", &ink);
    double prompt_top = top + height * 0.13;
    if (params->is_circular)
    {
        /* Lower the prompt until its left-aligned ink fits inside the circle. */
        const double radius = params->circle_radius;
        const double dx = fmax(fabs(left - center_x), fabs(left + ink.width - center_x));
        const double edge_y = params->circle_center_y - sqrt(fmax(0, radius * radius - dx * dx));
        prompt_top = fmax(prompt_top, edge_y + 2 * scale);
    }
    cairo_move_to(cr, left - ink.x_bearing, prompt_top - ink.y_bearing);
    cairo_show_text(cr, ">_");
}

int paint_terminal_overlay(cairo_t *cr, const Config *config,
                           const monitor_sensor_data_t *data,
                           const ScalingParams *params)
{
    const double height = params->is_circular ? params->circle_radius * 2.0
                                              : config->display_height;
    const double scale = params->is_circular ? height / 240.0 : params->scale_uniform;
    const double top = params->circle_center_y - height / 2.0;
    double first_row = 0.36;
    if (config->tty_icon == TTY_ICON_SMALL)
        first_row = params->is_circular ? 0.31 : 0.285;
    else if (config->tty_icon == TTY_ICON_OFF)
        first_row = params->is_circular ? 0.26 : 0.22;
    const double earliest = top + height * first_row;
    double last = top + height * (params->is_circular ? 0.84 : 0.90);
    if (config->display_show_clock)
        last = fmin(last, config->display_height * 0.90 - 15 * params->scale_uniform);
    const double step = fmin((last - earliest) / 4.0, config->display_width / 6.0);
    const double first = (earliest + last - 4 * step) / 2.0;
    const double font_size = step * 1.10;
    const double margin = config->display_width * 0.05;
    const double fallback_width = config->display_width - 2 * margin;
    double column_left = margin;
    double column_width = fallback_width;
    TerminalRow rows[5];
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        TerminalRow *row = &rows[i];
        row->baseline = first + i * step;
        row->font_size = font_size;
        double left, width;
        calculate_safe_region_bounds(params, row->baseline - font_size * 0.85,
                                     font_size * 1.10, 0.94, margin, fallback_width,
                                     &left, &width);
        column_left = fmax(column_left, left);
        column_width = fmin(column_width, width);
    }

    const double value_right = column_left + column_width * 0.73;
    const double bar_left = column_left + column_width * 0.78;
    const double bar_width = column_width * 0.22;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        rows[i].left = column_left;
        rows[i].value_right = value_right;
        rows[i].bar_left = bar_left;
        rows[i].bar_width = bar_width;
    }

    cairo_save(cr);
    draw_prompt(cr, config, params, column_left, top, height, scale);

    cairo_select_font_face(cr, config->tty_font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    if (config->tty_icon != TTY_ICON_OFF)
    {
        cairo_set_font_size(cr, fmax(13 * scale, font_size * 0.40));
        set_cairo_color(cr, &config->font_color_label);
        const double heading_y = first - font_size * 0.82;
        cairo_text_extents_t heading;
        cairo_text_extents(cr, "LOAD", &heading);
        double heading_left, heading_width;
        calculate_safe_region_bounds(params, heading_y + heading.y_bearing, heading.height,
                                     0.94, margin, fallback_width, &heading_left, &heading_width);
        const double heading_x = fmin(bar_left + bar_width / 2.0,
                                      heading_left + heading_width - heading.width / 2.0);
        draw_centered(cr, heading_x, heading_y, "LOAD");
    }
    draw_temperature(cr, config, data, &rows[0], "CPU", "cpu", 1);
    draw_temperature(cr, config, data, &rows[1], "GPU", "gpu", 1);
    draw_temperature(cr, config, data, &rows[2], "LIQ", "liquid", 0);
    draw_power(cr, config, data, &rows[3]);
    draw_memory(cr, config, &rows[4]);

    cairo_restore(cr);
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}
