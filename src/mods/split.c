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
#include <pthread.h>
#include <stdio.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

#include "../device/config.h"
#include "../srv/cc_sensor.h"
#include "display.h"
#include "split.h"
#include "ring.h"
#include "text.h"
#include "render.h"

enum
{
    SPLIT_LABEL,
    SPLIT_TEMPERATURE,
    SPLIT_LOAD,
    SPLIT_POWER,
    SPLIT_SPEED,
    SPLIT_WIDGETS_PER_COLUMN
};

static const CustomScreen *split_screen;
static pthread_once_t split_preset_once = PTHREAD_ONCE_INIT;

static void load_split_preset(void)
{
    static CustomLayout preset;
    if (!load_layout_preset("split", &preset))
        return;

    const CustomScreen *screen = &preset.screens[0];
    if (preset.screen_count != 1 || screen->arrangement != CUSTOM_COLUMNS ||
        screen->count != 1 + 2 * SPLIT_WIDGETS_PER_COLUMN ||
        screen->elements[0].type != CUSTOM_FRAME)
    {
        log_message(LOG_ERROR, "Split layout preset requires a divider and two columns");
        return;
    }

    const char *const sources[] = {"cpu", "gpu"};
    for (int i = 0; i < 2; i++)
    {
        const CustomElement *widgets = &screen->elements[1 + i * SPLIT_WIDGETS_PER_COLUMN];
        if (widgets[SPLIT_LABEL].type != CUSTOM_TEXT ||
            widgets[SPLIT_TEMPERATURE].type != CUSTOM_VALUE ||
            widgets[SPLIT_TEMPERATURE].channel != SENSOR_CATEGORY_TEMP ||
            strcmp(widgets[SPLIT_TEMPERATURE].source, sources[i]) != 0 ||
            widgets[SPLIT_LOAD].type != CUSTOM_VALUE || widgets[SPLIT_LOAD].channel != SENSOR_CATEGORY_DUTY ||
            widgets[SPLIT_POWER].type != CUSTOM_VALUE || widgets[SPLIT_POWER].channel != SENSOR_CATEGORY_WATTS ||
            widgets[SPLIT_SPEED].type != CUSTOM_VALUE || widgets[SPLIT_SPEED].channel != SENSOR_CATEGORY_RPM)
        {
            log_message(LOG_ERROR, "Invalid column in the Split layout preset");
            return;
        }
    }
    split_screen = screen;
}

static const CustomElement *split_column(int index)
{
    return &split_screen->elements[1 + index * SPLIT_WIDGETS_PER_COLUMN];
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
                                   const char *slot_value,
                                   char short_name[static SENSOR_DEVICE_NAME_LEN])
{
    const SensorConfig *sensor_config = get_sensor_config(config, slot_value);
    if (sensor_config && sensor_config->label[0] != '\0')
        return sensor_config->label;
    for (int i = 0; i < 2; i++)
    {
        const CustomElement *widgets = split_column(i);
        if (strcmp(slot_value, widgets[SPLIT_TEMPERATURE].source) == 0)
            return widgets[SPLIT_LABEL].text;
    }
    return get_slot_label(config, data, slot_value, short_name);
}

static void draw_split_pane(cairo_t *cr, const struct Config *config,
                            const monitor_sensor_data_t *data,
                            const ScalingParams *params,
                            const LayoutBox *pane, int column,
                            const char *slot_value)
{
    if (!slot_is_active(slot_value))
        return;

    const CustomElement *widgets = split_column(column);
    log_layout_box(cr, config, "column", pane->x, pane->y, pane->width, pane->height);
    const double text_width = pane->width * 0.96;
    const double min_dimension = fmin((double)config->display_width,
                                      (double)config->display_height);
    const double configured_temp_size = get_slot_font_size(config, slot_value);
    const double temp_size = configured_temp_size > 0.0f
                                 ? scale_value_avg(params, configured_temp_size)
                                 : min_dimension * 4.0;
    char short_name[SENSOR_DEVICE_NAME_LEN];
    const char *label = get_split_label(config, data, slot_value, short_name);
    const float temperature = get_slot_temperature(data, slot_value);
    char temperature_text[16] = {0};
    char secondary_text[3][24] = {{0}};
    const CustomElement *visible_widgets[SPLIT_WIDGETS_PER_COLUMN] = {
        &widgets[SPLIT_LABEL], &widgets[SPLIT_TEMPERATURE],
    };
    int count = SPLIT_LOAD;
    format_slot_value_text(temperature_text, sizeof(temperature_text), data,
                           slot_value, temperature);
    const int visible[] = {
        [SENSOR_CATEGORY_DUTY] = config->split_show_load,
        [SENSOR_CATEGORY_WATTS] = config->split_show_watts,
        [SENSOR_CATEGORY_RPM] = config->split_show_rpm,
    };
    static const char *const units[] = {
        [SENSOR_CATEGORY_DUTY] = "%",
        [SENSOR_CATEGORY_WATTS] = "W",
        [SENSOR_CATEGORY_RPM] = "RPM",
    };
    for (int i = SPLIT_LOAD; i < SPLIT_WIDGETS_PER_COLUMN; i++)
    {
        const sensor_category_t channel = widgets[i].channel;
        if (!visible[channel])
            continue;
        const char *channel_slot =
            channel == SENSOR_CATEGORY_RPM && strcmp(slot_value, "cpu") == 0
                ? split_column(0)[SPLIT_SPEED].source : slot_value;
        format_channel_value(secondary_text[count - SPLIT_LOAD],
                             sizeof(secondary_text[0]),
                             find_channel_sensor_for_slot(data, channel_slot,
                                                          channel), units[channel]);
        visible_widgets[count++] = &widgets[i];
    }

    LayoutBox boxes[SPLIT_WIDGETS_PER_COLUMN];
    layout_stack(pane, visible_widgets, count, boxes);
    const Color temperature_color = config->split_ring_by_sensor
                                        ? (Color){255, 255, 255, 1}
                                        : get_slot_bar_color(config, slot_value,
                                                             temperature);
    const double offset_x = scale_value_x(
        params, (double)get_slot_offset_x(config, slot_value));
    const double offset_y = scale_value_y(
        params, (double)get_slot_offset_y(config, slot_value));
    const double center_x = pane->x + pane->width / 2.0 + offset_x;
    const LayoutBox *temp = &boxes[SPLIT_TEMPERATURE];
    const LayoutBox *label_box = &boxes[SPLIT_LABEL];
    const char *face = config->font_face[0] ? config->font_face : widgets[SPLIT_TEMPERATURE].font_face;
    cairo_select_font_face(cr, face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    const double rendered_temp_size = render_centered_temperature(
        cr, temperature_text, center_x,
        temp->y + temp->height * 0.5 + offset_y,
        temp_size, pane->width * 0.99, temp->height,
        &temperature_color, params);
    const double label_ratio = (double)widgets[SPLIT_LABEL].font_size / widgets[SPLIT_TEMPERATURE].font_size;
    const double secondary_ratio = (double)widgets[SPLIT_POWER].font_size / widgets[SPLIT_TEMPERATURE].font_size;
    const double label_size = fmax(1.0, rendered_temp_size * label_ratio);
    const double secondary_size = config->font_size_watts > 0.0f
                                      ? scale_value_avg(params,
                                                        config->font_size_watts)
                                      : rendered_temp_size * secondary_ratio;
    render_centered_text(cr, label ? label : "???", center_x,
                       label_box->y + label_box->height * 0.5 + offset_y,
                       label_size, text_width, label_box->height,
                       &config->font_color_label);
    for (int i = SPLIT_LOAD; i < count; i++)
    {
        const LayoutBox *row = &boxes[i];
        const double row_size = visible_widgets[i]->channel == SENSOR_CATEGORY_DUTY
                                    ? secondary_size * secondary_ratio
                                    : secondary_size;
        render_centered_value_with_small_unit(
            cr, secondary_text[i - SPLIT_LOAD], center_x,
            row->y + row->height * 0.5 + offset_y,
            fmax(1.0, row_size), text_width, row->height,
            &config->font_color_label, params);
    }
}

static void get_split_slots(const struct Config *config, const char *slots[2])
{
    if (!config)
        return;

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
            slots[active_count++] = configured_slots[i];
    }
}

static int paint_split(cairo_t *cr, const Config *config,
                       const monitor_sensor_data_t *data,
                       const ScalingParams *params, const char *const slots[2])
{
    pthread_once(&split_preset_once, load_split_preset);
    if (!split_screen)
        return 0;

    const unsigned occupied = (slots[0] ? 1U : 0U) | (slots[1] ? 2U : 0U);
    LayoutBox columns[2], divider;
    if (!layout_columns(config, params, occupied, columns, &divider))
    {
        log_message(LOG_ERROR, "Split mode: display is too small for two columns");
        return 0;
    }

    if (config->display_rotation == 0 || params->is_circular)
    {
        if (config->split_ring_by_sensor)
            draw_split_sensor_ring(cr, config, data, params,
                                   slots[0], slots[1]);
        else
            draw_sensor_ring(cr, config, data, params);
    }

    for (int i = 0; i < 2; i++)
        draw_split_pane(cr, config, data, params, &columns[i], i, slots[i]);
    render_divider(cr, config, &divider, &config->font_color_label);
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

int paint_split_overlay(cairo_t *cr, const Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params)
{
    const char *slots[2] = {NULL, NULL};
    get_split_slots(config, slots);
    return paint_split(cr, config, data, params, slots);
}

int render_split_preview(const struct Config *config,
                         const monitor_sensor_data_t *data,
                         const char *device_name)
{
    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);

    const char *slots[2] = {NULL, NULL};
    get_split_slots(config, slots);
    const FrameRing ring = {
        .first_slot = config->split_ring_by_sensor ? slots[0] : config->sensor_ring_sensor,
        .second_slot = slots[1],
        .split = config->split_ring_by_sensor,
    };
    return render_frame(config, data, &params, paint_split_overlay, &ring);
}
