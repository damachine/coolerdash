/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Split mode rendering (two bar-free sensor columns).
 * @details Renders the first two active slots from left to right, separated by
 * a centered vertical line. Temperature text uses the configured sensor
 * threshold color; labels and secondary values use the configured label color.
 */

// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "../srv/cc_sensor.h"
#include "display.h"
#include "split.h"

typedef struct
{
    double x;
    double width;
    double center_x;
} SplitPane;

typedef struct
{
    double top;
    double bottom;
    double divider_x;
    SplitPane left;
    SplitPane right;
} SplitLayout;

static void draw_centered_text(cairo_t *cr, const char *text,
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

static void draw_centered_value_with_small_unit(
    cairo_t *cr, const char *text, double center_x, double center_y,
    double preferred_size, double max_width, double max_height,
    const Color *color, const ScalingParams *params)
{
    if (!cr || !text || text[0] == '\0' || !color || !params ||
        max_width <= 0.0 || max_height <= 0.0)
        return;

    const size_t text_len = strlen(text);
    const char unit = text_len > 0 ? text[text_len - 1] : '\0';
    if (unit != '%' && unit != 'W')
    {
        draw_centered_text(cr, text, center_x, center_y, preferred_size,
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
    double unit_size = number_size * 0.5;
    const double unit_gap = fmax(1.0, scale_value_avg(params, 1.0));
    cairo_text_extents_t number_ext = {0};
    cairo_text_extents_t unit_ext = {0};
    double number_width = 0.0;
    double total_width = 0.0;
    double content_height = 0.0;
    char unit_text[2] = {unit, '\0'};

    while (number_size > 1.0)
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
        if (total_width <= max_width && content_height <= max_height)
            break;
        number_size *= 0.92;
    }

    number_size = fmax(1.0, number_size);
    unit_size = number_size * 0.5;
    cairo_set_font_size(cr, number_size);
    cairo_text_extents(cr, number, &number_ext);
    cairo_set_font_size(cr, unit_size);
    cairo_text_extents(cr, unit_text, &unit_ext);
    number_width = fmax(number_ext.x_advance, number_ext.width);
    total_width = number_width + unit_gap +
                  fmax(unit_ext.x_advance, unit_ext.width);
    content_height =
        fmax(number_ext.height,
             number_ext.height * 0.08 + unit_ext.height);

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

static double draw_centered_temperature(cairo_t *cr, const char *number,
                                        double center_x, double center_y,
                                        double preferred_size, double max_width,
                                        double max_height, const Color *color,
                                        const ScalingParams *params)
{
    if (!cr || !number || number[0] == '\0' || !color || !params ||
        max_width <= 0.0 || max_height <= 0.0)
        return 0.0;

    double number_size = fmax(6.0, preferred_size);
    double degree_size = number_size * 0.50;
    const double degree_gap = fmax(0.5, scale_value_avg(params, 0.5));
    cairo_text_extents_t number_ext = {0};
    cairo_text_extents_t degree_ext = {0};
    cairo_font_extents_t number_font_ext = {0};
    double total_width = 0.0;

    while (number_size > 1.0)
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
        if (total_width <= max_width &&
            number_font_ext.ascent + number_font_ext.descent <= max_height)
            break;

        number_size *= 0.92;
    }

    cairo_set_font_size(cr, number_size);
    cairo_text_extents(cr, number, &number_ext);
    cairo_font_extents(cr, &number_font_ext);
    degree_size = number_size * 0.50;
    cairo_set_font_size(cr, degree_size);
    cairo_text_extents(cr, "\xC2\xB0", &degree_ext);
    total_width = fmax(number_ext.x_advance, number_ext.width) + degree_gap +
                  fmax(degree_ext.x_advance, degree_ext.width);
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

static int calculate_split_layout(const struct Config *config,
                                  const ScalingParams *params,
                                  SplitLayout *layout)
{
    if (!config || !params || !layout || config->display_width == 0 ||
        config->display_height == 0)
        return 0;

    memset(layout, 0, sizeof(*layout));

    LayoutContext geometry = {0};
    if (!calculate_layout_context(config, params, &geometry))
        return 0;

    const double display_width = config->display_width;
    const double available_height = geometry.height;
    const double vertical_inset = available_height *
                                  (params->is_circular ? 0.15 : 0.07);
    layout->top = geometry.top + vertical_inset;
    layout->bottom = geometry.bottom - vertical_inset;
    layout->divider_x = geometry.center_x;

    double safe_x = params->safe_content_margin;
    double safe_width = params->safe_bar_width;
    calculate_safe_region_bounds(params, layout->top,
                                 layout->bottom - layout->top,
                                 params->is_circular ? 0.94 : 0.96,
                                 safe_x, safe_width, &safe_x, &safe_width);

    const double outer_padding = fmax(scale_value_avg(params, 3.0),
                                      display_width * 0.018);
    const double center_gap = fmax(scale_value_avg(params, 5.0),
                                   display_width * 0.025);
    const double safe_right = safe_x + safe_width;

    layout->left.x = safe_x + outer_padding;
    layout->left.width = layout->divider_x - center_gap - layout->left.x;
    layout->right.x = layout->divider_x + center_gap;
    layout->right.width = safe_right - outer_padding - layout->right.x;

    if (layout->left.width < 12.0 || layout->right.width < 12.0)
        return 0;

    layout->left.center_x = layout->left.x + layout->left.width / 2.0;
    layout->right.center_x = layout->right.x + layout->right.width / 2.0;
    return 1;
}

static void format_channel_value(char *buffer, size_t buffer_size,
                                 const sensor_entry_t *entry,
                                 const char *fallback_unit)
{
    if (!buffer || buffer_size == 0)
        return;

    if (!entry)
    {
        snprintf(buffer, buffer_size, "\xE2\x80\x94");
        return;
    }

    const char *unit = entry->unit[0] ? entry->unit : fallback_unit;
    snprintf(buffer, buffer_size, "%.0f%s%s", entry->value,
             strcmp(unit, "%") == 0 ? "" : " ", unit);
}

static const char *get_split_label(const struct Config *config,
                                   const monitor_sensor_data_t *data,
                                   const char *slot_value)
{
    const SensorConfig *sensor_config = get_sensor_config(config, slot_value);
    if (sensor_config && sensor_config->label[0] != '\0')
        return sensor_config->label;
    if (strcmp(slot_value, "cpu") == 0)
        return "CPU";
    if (strcmp(slot_value, "gpu") == 0)
        return "GPU";
    return get_slot_label(config, data, slot_value);
}

static void draw_split_pane(cairo_t *cr, const struct Config *config,
                            const monitor_sensor_data_t *data,
                            const ScalingParams *params,
                            const SplitLayout *layout, const SplitPane *pane,
                            const char *slot_value)
{
    if (!slot_is_active(slot_value))
        return;

    const double content_height = layout->bottom - layout->top;
    const double text_width = pane->width * 0.96;
    const double min_dimension = fmin((double)config->display_width,
                                      (double)config->display_height);
    const double configured_temp_size = get_slot_font_size(config, slot_value);
    const double temp_size = configured_temp_size > 0.0f
                                 ? scale_value_avg(params, configured_temp_size)
                                 : min_dimension * 4.0;
    const char *label = get_split_label(config, data, slot_value);
    const float temperature = get_slot_temperature(data, slot_value);
    char temperature_text[16] = {0};
    char duty_text[24] = {0};
    char watts_text[24] = {0};
    format_slot_value_text(temperature_text, sizeof(temperature_text), data,
                           slot_value, temperature);
    format_channel_value(
        duty_text, sizeof(duty_text),
        find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_DUTY),
        "%");
    format_channel_value(
        watts_text, sizeof(watts_text),
        find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_WATTS),
        "W");

    const Color temperature_color =
        get_slot_bar_color(config, slot_value, temperature);
    const double offset_x = scale_value_x(
        params, (double)get_slot_offset_x(config, slot_value));
    const double offset_y = scale_value_y(
        params, (double)get_slot_offset_y(config, slot_value));
    const double center_x = pane->center_x + offset_x;
    const double region_gap = fmax(1.0, content_height * 0.01);
    const double label_top = layout->top;
    const double label_height = content_height * 0.20;
    const double temp_top = label_top + label_height + region_gap;
    const double temp_height = content_height * 0.37;
    const double duty_top = temp_top + temp_height + region_gap;
    const double duty_height = content_height * 0.16;
    const double watts_top = duty_top + duty_height + region_gap;
    const double watts_height = fmax(1.0, layout->bottom - watts_top);
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    const double rendered_temp_size = draw_centered_temperature(
        cr, temperature_text, center_x,
        temp_top + temp_height * 0.5 + offset_y,
        temp_size, pane->width * 0.99, temp_height,
        &temperature_color, params);
    const double label_size = fmax(1.0, rendered_temp_size * 0.50);
    const double secondary_size = fmax(1.0, rendered_temp_size * 0.66);
    draw_centered_text(cr, label ? label : "???", center_x,
                       label_top + label_height * 0.5 + offset_y,
                       label_size, text_width, label_height,
                       &config->font_color_label);
    draw_centered_value_with_small_unit(
        cr, duty_text, center_x,
        duty_top + duty_height * 0.5 + offset_y,
        secondary_size, text_width, duty_height,
        &config->font_color_temp, params);
    draw_centered_value_with_small_unit(
        cr, watts_text, center_x,
        watts_top + watts_height * 0.5 + offset_y,
        secondary_size, text_width, watts_height,
        &config->font_color_temp, params);
}

static int render_split_display(const struct Config *config,
                                const monitor_sensor_data_t *data,
                                const char *device_name)
{
    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);

    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;

    SplitLayout layout = {0};
    if (!calculate_split_layout(config, &params, &layout))
    {
        log_message(LOG_ERROR, "Split mode: display is too small for two columns");
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    paint_display_background(cr, config);

    const char *active_slots[2] = {NULL, NULL};
    const char *configured_slots[] = {
        config->sensor_slot_1,
        config->sensor_slot_2,
        config->sensor_slot_3,
    };
    int active_count = 0;
    for (size_t i = 0; i < sizeof(configured_slots) / sizeof(configured_slots[0]) &&
                       active_count < 2;
         i++)
    {
        if (slot_is_active(configured_slots[i]))
            active_slots[active_count++] = configured_slots[i];
    }

    if (active_count > 0)
        draw_split_pane(cr, config, data, &params, &layout, &layout.left,
                        active_slots[0]);
    if (active_count > 1)
        draw_split_pane(cr, config, data, &params, &layout, &layout.right,
                        active_slots[1]);

    cairo_save(cr);
    cairo_set_line_width(cr, fmax(1.0, scale_value_avg(&params, 1.4)));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    set_cairo_color_alpha(cr, &config->font_color_label, 0.62);
    cairo_move_to(cr, layout.divider_x, layout.top);
    cairo_line_to(cr, layout.divider_x, layout.bottom);
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_surface_flush(surface);
    int success = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
    if (success)
    {
        success = cairo_surface_write_to_png(surface,
                                             config->paths_image_coolerdash) ==
                  CAIRO_STATUS_SUCCESS;
    }

    if (!success)
        log_message(LOG_ERROR, "Split display rendering failed");

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return success;
}

void draw_split_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config for split mode");
        return;
    }

    monitor_sensor_data_t data = {0};
    if (!get_sensor_monitor_data(config, &data))
    {
        log_message(LOG_WARNING, "Split mode: failed to get sensor data");
        return;
    }

    char device_uid[128] = {0};
    char device_name[256] = {0};
    int screen_width = 0;
    int screen_height = 0;
    const bool device_available = get_cached_lcd_device_data(
        config, device_uid, sizeof(device_uid), device_name,
        sizeof(device_name), &screen_width, &screen_height);

    if (!render_split_display(config, &data, device_name))
        return;

    if (is_session_initialized() && device_available && device_uid[0] != '\0')
    {
        const char *name = device_name[0] ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending split image to LCD: %s [%s]", name,
                    device_uid);
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);
    }
    else
    {
        log_message(LOG_WARNING,
                    "Skipping split LCD upload - device not available");
    }
}
