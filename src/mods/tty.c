/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "tty.h"
#include "bar.h"
#include "readings.h"

static const Color terminal_accent = {149, 185, 251, 1};
static const double label_scale = 0.68;
static const double fraction_scale = 0.62;
static const double unit_scale = 0.64;

typedef struct
{
    const CustomElement *label, *value, *bar;
} TerminalWidgets;

static pthread_once_t tty_preset_once = PTHREAD_ONCE_INIT;
static const CustomElement *tty_prompt, *tty_heading;
static TerminalWidgets tty_widgets[CUSTOM_LAYOUT_MAX_ELEMENTS / 2];
static int tty_row_count;

static void load_terminal_preset(void)
{
    static CustomLayout preset;
    if (!load_layout_preset("terminal", &preset))
        return;
    const CustomScreen *screen = &preset.screens[0];
    if (preset.screen_count != 1 || screen->count < 4 ||
        screen->elements[0].type != CUSTOM_TEXT || screen->elements[1].type != CUSTOM_TEXT)
    {
        log_message(LOG_ERROR, "Invalid header in the TTY layout preset");
        return;
    }

    int count = 0;
    for (int i = 2; i < screen->count;)
    {
        const CustomElement *label = &screen->elements[i++];
        if (i == screen->count || label->type != CUSTOM_TEXT ||
            screen->elements[i].type != CUSTOM_VALUE)
        {
            log_message(LOG_ERROR, "Invalid row in the TTY layout preset");
            return;
        }
        const CustomElement *value = &screen->elements[i++];
        const CustomElement *bar = NULL;
        if (i < screen->count && screen->elements[i].type == CUSTOM_BAR)
            bar = &screen->elements[i++];
        tty_widgets[count++] = (TerminalWidgets){label, value, bar};
    }
    tty_prompt = &screen->elements[0];
    tty_heading = &screen->elements[1];
    tty_row_count = count;
}

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
    cairo_text_extents_t fraction;
    cairo_text_extents_t unit;
    double value_width;
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
                               const char *value, const char *fraction, const char *unit)
{
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, value, &text->value);
    text->value_width = text->value.width;
    if (*fraction)
    {
        cairo_set_font_size(cr, size * fraction_scale);
        cairo_text_extents(cr, fraction, &text->fraction);
        text->value_width += text->fraction.width + size * 0.03;
    }
    cairo_set_font_size(cr, unit_size);
    cairo_text_extents(cr, unit, &text->unit);
    text->unit_width = *unit ? text->unit.width + size * 0.12 : 0;
    return text->label.width + text->value_width + text->unit_width + size * 0.34;
}

static void draw_row(cairo_t *cr, const Config *config, const TerminalRow *row, const char *label,
                     const char *value, const char *fraction, const char *unit,
                     const CustomElement *bar, double progress)
{
    TerminalText text;
    const int temperature = strcmp(unit, "°") == 0;
    const double label_size = row->font_size * label_scale;
    double font_size = row->font_size;
    double unit_size = font_size * unit_scale * (temperature ? 1.75 : 1);
    const double available = row->value_right - row->left;
    cairo_set_font_size(cr, label_size);
    cairo_text_extents(cr, label, &text.label);
    const double span = measure_row_text(cr, &text, font_size, unit_size, value, fraction, unit);
    if (span > available)
    {
        const double fixed_width = text.label.width + (temperature ? text.unit.width : 0);
        font_size = fmax(1, floor(font_size * (available - fixed_width) / (span - fixed_width)));
        if (!temperature)
            unit_size = font_size * unit_scale;
        measure_row_text(cr, &text, font_size, unit_size, value, fraction, unit);
    }

    double value_top = row->baseline + text.value.y_bearing;
    if (!*unit)
    {
        cairo_text_extents_t digit;
        cairo_set_font_size(cr, font_size);
        cairo_text_extents(cr, "0", &digit);
        value_top = row->baseline + digit.y_bearing;
    }
    set_cairo_color(cr, &config->font_color_label);
    cairo_set_font_size(cr, label_size);
    cairo_move_to(cr, row->left - text.label.x_bearing, value_top - text.label.y_bearing);
    cairo_show_text(cr, label);

    set_cairo_color(cr, &config->font_color_temp);
    const double value_end = row->value_right - text.unit_width;
    cairo_set_font_size(cr, font_size);
    cairo_move_to(cr, value_end - text.value_width - text.value.x_bearing, row->baseline);
    cairo_show_text(cr, value);
    if (*fraction)
    {
        cairo_set_font_size(cr, font_size * fraction_scale);
        cairo_move_to(cr, value_end - text.fraction.width - text.fraction.x_bearing, row->baseline);
        cairo_show_text(cr, fraction);
    }
    if (*unit)
    {
        const double unit_baseline = temperature ? value_top - text.unit.y_bearing : row->baseline;
        cairo_set_font_size(cr, unit_size);
        cairo_move_to(cr, row->value_right - text.unit.x_bearing - text.unit.width, unit_baseline);
        cairo_show_text(cr, unit);
    }

    if (bar)
    {
        const double bar_height = fmin(row->font_size * 0.35, row->bar_width / 3.0);
        const double bar_y = row->baseline - bar_height * 1.18;
        log_layout_box(cr, config, "bar", row->bar_left, bar_y, row->bar_width, bar_height);
        draw_segment_bar(cr, row->bar_left, bar_y,
                         row->bar_width, bar_height, bar->segments,
                         fmax(0, fmin(1, progress)), &bar->color);
    }
}

static double terminal_progress(const Config *config, const monitor_sensor_data_t *data,
                                 const CustomElement *bar, WidgetReadings *readings)
{
    const sensor_entry_t *sensor = bar ? get_widget_reading(config, data, bar, readings) : NULL;
    return sensor ? widget_progress(bar, sensor->value) : 0.0;
}

static void draw_sensor_value(cairo_t *cr, const Config *config, const monitor_sensor_data_t *data,
                             const TerminalRow *row, const TerminalWidgets *widgets)
{
    WidgetReadings readings = {0};
    const char *slot = widgets->value->source;
    const sensor_entry_t *sensor = get_widget_reading(config, data, widgets->value, &readings);
    char value[24] = "—";
    char fraction[sizeof(value)] = "";
    char unit[SENSOR_UNIT_LEN] = "";
    if (sensor)
    {
        const char *display_unit = widgets->value->channel == SENSOR_CATEGORY_TEMP &&
            get_custom_source(slot) == CUSTOM_SOURCE_SENSOR && get_slot_is_temp(data, slot)
            ? "°" : sensor->unit;
        snprintf(unit, sizeof(unit), "%s", display_unit);
        snprintf(value, sizeof(value), "%.*f", sensor->use_decimal ? 1 : 0, sensor->value);
        if (strcmp(slot, "liquid") == 0)
        {
            char *decimal = strpbrk(value, ".,");
            if (decimal)
            {
                snprintf(fraction, sizeof(fraction), "%s", decimal);
                *decimal = '\0';
            }
        }
    }
    const CustomElement *bar = widgets->bar;
    draw_row(cr, config, row, widgets->label->text, value, fraction,
             unit, bar, terminal_progress(config, data, bar, &readings));
}

static void draw_power(cairo_t *cr, const Config *config, const monitor_sensor_data_t *data,
                       const TerminalRow *row, const TerminalWidgets *widgets)
{
    const char *label;
    double watts;
    char value[24] = "—";
    const int available = read_processor_power(data, &watts, &label);
    if (available)
        snprintf(value, sizeof(value), "%.0f", watts);
    if (strcmp(label, "PWR") == 0)
        label = widgets->label->text;
    WidgetReadings readings = {0};
    draw_row(cr, config, row, label, value, "", available ? "W" : "", widgets->bar,
             terminal_progress(config, data, widgets->bar, &readings));
}

static void draw_memory(cairo_t *cr, const Config *config, const monitor_sensor_data_t *data,
                         const TerminalRow *row, const TerminalWidgets *widgets)
{
    WidgetReadings readings = {0};
    char value[24] = "—";
    const int available = read_used_memory(config, &readings.gib, &readings.memory_load);
    readings.memory_status = available ? 1 : -1;
    if (available)
        snprintf(value, sizeof(value), "%.1f", readings.gib);
    const CustomElement *bar = widgets->bar;
    const double progress = available && bar && get_custom_source(bar->source) == CUSTOM_SOURCE_RAM_LOAD
        ? widget_progress(bar, readings.memory_load * 100.0)
        : terminal_progress(config, data, bar, &readings);
    draw_row(cr, config, row, widgets->label->text, value, "",
             available ? "GiB" : "", widgets->bar, progress);
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
    cairo_set_font_size(cr, (large ? tty_prompt->font_size : 16) * scale);
    if (large)
    {
        draw_centered(cr, center_x, tile_y + 6 * scale, tty_prompt->text);
        return;
    }

    cairo_text_extents_t ink;
    cairo_text_extents(cr, tty_prompt->text, &ink);
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
    cairo_show_text(cr, tty_prompt->text);
}

int paint_terminal_overlay(cairo_t *cr, const Config *config,
                           const monitor_sensor_data_t *data,
                           const ScalingParams *params)
{
    pthread_once(&tty_preset_once, load_terminal_preset);
    if (!tty_row_count)
        return 0;

    const double height = params->is_circular ? params->circle_radius * 2.0
                                              : config->display_height;
    const double scale = params->is_circular ? height / 240.0 : params->scale_uniform;
    const double top = params->circle_center_y - height / 2.0;
    double first_row = 0.36;
    if (config->tty_icon == TTY_ICON_SMALL)
        first_row = 0.31;
    else if (config->tty_icon == TTY_ICON_OFF)
        first_row = params->is_circular ? 0.26 : 0.22;
    const double earliest = top + height * first_row;
    double last = top + height * (params->is_circular ? 0.84 : 0.90);
    if (config->display_show_clock)
        last = fmin(last, config->display_height * 0.90 - 15 * params->scale_uniform);
    const int intervals = tty_row_count - 1;
    const double step = fmin((last - earliest) / (intervals > 0 ? intervals : 1),
                             config->display_width / 6.0);
    const double first = (earliest + last - intervals * step) / 2.0;
    const double font_size = step * 1.10;
    const double margin = config->display_width * 0.05;
    const double fallback_width = config->display_width - 2 * margin;
    double column_left = margin;
    double column_width = fallback_width;
    TerminalRow rows[CUSTOM_LAYOUT_MAX_ELEMENTS / 2];
    for (int i = 0; i < tty_row_count; i++)
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
    for (int i = 0; i < tty_row_count; i++)
    {
        rows[i].left = column_left;
        rows[i].value_right = value_right;
        rows[i].bar_left = bar_left;
        rows[i].bar_width = bar_width;
        log_layout_box(cr, config, "row", column_left, rows[i].baseline - font_size * 0.85,
                       column_width, font_size * 1.10);
    }

    cairo_save(cr);
    draw_prompt(cr, config, params, column_left, top, height, scale);

    cairo_select_font_face(cr, config->tty_font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    if (config->tty_icon != TTY_ICON_OFF)
    {
        cairo_set_font_size(cr, fmax(tty_heading->font_size * scale, font_size * 0.40));
        set_cairo_color(cr, &config->font_color_label);
        const double heading_y = first - font_size * 0.82;
        cairo_text_extents_t heading;
        cairo_text_extents(cr, tty_heading->text, &heading);
        double heading_left, heading_width;
        calculate_safe_region_bounds(params, heading_y + heading.y_bearing, heading.height,
                                     0.94, margin, fallback_width, &heading_left, &heading_width);
        const double heading_x = fmin(bar_left + bar_width / 2.0,
                                      heading_left + heading_width - heading.width / 2.0);
        draw_centered(cr, heading_x, heading_y, tty_heading->text);
    }
    for (int i = 0; i < tty_row_count; i++)
    {
        const TerminalWidgets *widgets = &tty_widgets[i];
        switch (get_custom_source(widgets->value->source))
        {
        case CUSTOM_SOURCE_POWER:
            draw_power(cr, config, data, &rows[i], widgets);
            break;
        case CUSTOM_SOURCE_RAM_USED:
            draw_memory(cr, config, data, &rows[i], widgets);
            break;
        default:
            draw_sensor_value(cr, config, data, &rows[i], widgets);
            break;
        }
    }

    cairo_restore(cr);
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}
