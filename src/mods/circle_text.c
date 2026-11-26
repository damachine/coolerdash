/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "circle_text.h"

static int ascii_tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

static int contains_ignore_case(const char *haystack, const char *needle)
{
    if (!haystack || !needle || needle[0] == '\0')
        return 0;

    for (const char *h = haystack; *h; h++)
    {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np && ascii_tolower((unsigned char)*hp) ==
                                 ascii_tolower((unsigned char)*np))
        {
            hp++;
            np++;
        }
        if (*np == '\0')
            return 1;
    }

    return 0;
}

/**
 * @brief Find pump RPM sensor for a Liquidctl device.
 * @details Searches for an RPM sensor whose name contains "pump"
 * (case-insensitive). Falls back to first RPM sensor if no pump found.
 * @param data Sensor data collection
 * @return Pointer to pump RPM sensor, or NULL if not found
 */
static const sensor_entry_t *find_liquid_pump_rpm(
    const monitor_sensor_data_t *data)
{
    if (!data)
        return NULL;

    const sensor_entry_t *first_rpm = NULL;
    for (int i = 0; i < data->sensor_count; i++)
    {
        if (data->sensors[i].category != SENSOR_CATEGORY_RPM)
            continue;
        if (strcmp(data->sensors[i].device_type, "Liquidctl") != 0)
            continue;

        /* Prefer sensor with "pump" in the name. */
        if (contains_ignore_case(data->sensors[i].name, "pump"))
            return &data->sensors[i];

        if (!first_rpm)
            first_rpm = &data->sensors[i];
    }

    return first_rpm;
}

static void format_frequency(char *buf, size_t buf_size, float mhz)
{
    if (mhz >= 1000.0f)
        snprintf(buf, buf_size, "%.1f GHz", mhz / 1000.0f);
    else
        snprintf(buf, buf_size, "%.0f MHz", mhz);
}

static int format_freq_watts(const sensor_entry_t *freq,
                             const sensor_entry_t *watts,
                             char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0 || (!freq && !watts))
        return 0;

    if (freq && watts)
    {
        char freq_buf[24] = {0};
        format_frequency(freq_buf, sizeof(freq_buf), freq->value);
        snprintf(buf, buf_size, "%s  %.0fW", freq_buf, watts->value);
    }
    else if (freq)
    {
        format_frequency(buf, buf_size, freq->value);
    }
    else
    {
        snprintf(buf, buf_size, "%.0fW", watts->value);
    }

    return 1;
}

/**
 * @brief Build extra info text (freq/watts/RPM) for a slot.
 * @details Maps slot type to channel sensors:
 *   CPU → freq + watts ("X.X GHz  XXW")
 *   GPU → freq + watts ("XXXX MHz  XXW")
 *   Liquid → RPM ("XXXX RPM")
 *   Dynamic → tries freq, then watts, then RPM
 * @param data Sensor data collection
 * @param slot_value Slot configuration value
 * @param buf Output buffer
 * @param buf_size Output buffer size
 * @return 1 if text was written, 0 if no data available
 */
int circle_extra_info_text(const Config *config,
                               const monitor_sensor_data_t *data,
                               const char *slot_value,
                               char *buf, size_t buf_size)
{
    if (!config || !data || !slot_value || !buf || buf_size == 0)
        return 0;

    buf[0] = '\0';

    if (strcmp(slot_value, "cpu") == 0)
    {
        const sensor_entry_t *freq = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_FREQ);
        if (!config->circle_show_frequency) freq = NULL;
        const sensor_entry_t *watts = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_WATTS);
        if (!config->circle_show_watts) watts = NULL;
        return format_freq_watts(freq, watts, buf, buf_size);
    }

    if (strcmp(slot_value, "gpu") == 0)
    {
        const sensor_entry_t *freq = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_FREQ);
        if (!config->circle_show_frequency) freq = NULL;
        const sensor_entry_t *watts = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_WATTS);
        if (!config->circle_show_watts) watts = NULL;
        return format_freq_watts(freq, watts, buf, buf_size);
    }

    if (strcmp(slot_value, "liquid") == 0)
    {
        const sensor_entry_t *rpm = find_liquid_pump_rpm(data);
        if (!config->circle_show_rpm || !rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    /* Dynamic slot: try freq → watts → rpm */
    const sensor_entry_t *s = find_channel_sensor_for_slot(
        data, slot_value, SENSOR_CATEGORY_FREQ);
    if (s && config->circle_show_frequency)
    {
        format_frequency(buf, buf_size, s->value);
        return 1;
    }

    s = find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_WATTS);
    if (s && config->circle_show_watts)
    {
        snprintf(buf, buf_size, "%.0fW", s->value);
        return 1;
    }

    s = find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_RPM);
    if (s && config->circle_show_rpm)
    {
        snprintf(buf, buf_size, "%.0f RPM", s->value);
        return 1;
    }

    return 0;
}

/**
 * @brief Build second line of extra info (fan RPM for CPU/GPU).
 * @details CPU fan RPM comes from the Liquidctl device (AIO cooler),
 * since the CPU device itself has no fan sensor. GPU fan RPM comes
 * from the GPU device directly.
 * @param data Sensor data collection
 * @param slot_value Slot configuration value
 * @param buf Output buffer
 * @param buf_size Output buffer size
 * @return 1 if text was written, 0 if no data available
 */
int circle_extra_info_line2(const monitor_sensor_data_t *data,
                                const char *slot_value,
                                char *buf, size_t buf_size)
{
    if (!data || !slot_value || !buf || buf_size == 0)
        return 0;

    buf[0] = '\0';

    /* CPU: fan RPM comes from Liquidctl (AIO cooler fan, not pump) */
    if (strcmp(slot_value, "cpu") == 0)
    {
        const sensor_entry_t *rpm = find_channel_sensor_for_slot(
            data, "liquid", SENSOR_CATEGORY_RPM);
        if (!rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    /* GPU: fan RPM from the GPU device itself */
    if (strcmp(slot_value, "gpu") == 0)
    {
        const sensor_entry_t *rpm = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_RPM);
        if (!rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    return 0;
}

void circle_duty_layout(cairo_t *cr, const struct Config *config,
                                  const ScalingParams *params,
                                  const monitor_sensor_data_t *data,
                                  const char *slot_value, double max_width,
                                  double max_height,
                                  double preferred_font_size,
                                  DutyLayout *layout)
{
    if (!cr || !config || !params || !data || !slot_value || !layout)
        return;

    memset(layout, 0, sizeof(*layout));
    if (strcmp(slot_value, "cpu") != 0 && strcmp(slot_value, "gpu") != 0)
        return;

    const sensor_entry_t *duty = find_channel_sensor_for_slot(
        data, slot_value, SENSOR_CATEGORY_DUTY);
    if (!duty)
        return;

    snprintf(layout->number, sizeof(layout->number), "%.0f", duty->value);
    const double configured_temp = get_slot_font_size(config, slot_value);
    const double preferred_temp =
        configured_temp > 0.0f
            ? scale_value_avg(params, configured_temp)
            : fmax(scale_value_avg(params, 220.0), max_height * 7.4);
    layout->font_size = preferred_font_size > 0.0
                            ? preferred_font_size
                            : (config->font_size_duty > 0.0f
                                   ? scale_value_avg(params,
                                                     config->font_size_duty)
                                   : preferred_temp * 0.5);
    layout->spacing = fmax(1.0, scale_value_avg(params, 1.0));

    while (layout->font_size > 1.0)
    {
        layout->percent_font_size = layout->font_size / 2.05;
        cairo_set_font_size(cr, layout->font_size);
        cairo_text_extents(cr, layout->number, &layout->number_ext);
        cairo_set_font_size(cr, layout->percent_font_size);
        cairo_text_extents(cr, "%", &layout->percent_ext);

        layout->number_width =
            fmax(layout->number_ext.x_advance, layout->number_ext.width);
        layout->percent_width =
            fmax(layout->percent_ext.x_advance, layout->percent_ext.width);
        layout->total_width = layout->number_width + layout->spacing +
                              layout->percent_width;
        const double height =
            fmax(layout->number_ext.height,
                 layout->number_ext.height * 0.08 + layout->percent_ext.height);
        if (layout->total_width <= max_width && height <= max_height)
            break;
        layout->font_size *= 0.92;
    }

    layout->font_size = fmax(1.0, layout->font_size);
    layout->percent_font_size = layout->font_size / 2.05;
    cairo_set_font_size(cr, layout->font_size);
    cairo_text_extents(cr, layout->number, &layout->number_ext);
    cairo_set_font_size(cr, layout->percent_font_size);
    cairo_text_extents(cr, "%", &layout->percent_ext);
    layout->number_width =
        fmax(layout->number_ext.x_advance, layout->number_ext.width);
    layout->percent_width =
        fmax(layout->percent_ext.x_advance, layout->percent_ext.width);
    layout->total_width = layout->number_width + layout->spacing +
                          layout->percent_width;
    layout->active = layout->total_width > 0.0;
}

/**
 * @brief Check if text at position starts with a known unit suffix.
 * @details Recognises MHz, GHz, RPM (3-char) and W (1-char, only when
 * followed by end-of-string, space, or another separator).
 * @param text Pointer into the string to test
 * @return Length of matched unit token, or 0 if no match
 */
static int match_unit_at(const char *text)
{
    if (strncmp(text, "MHz", 3) == 0)
        return 3;
    if (strncmp(text, "GHz", 3) == 0)
        return 3;
    if (strncmp(text, "RPM", 3) == 0)
        return 3;
    if (text[0] == 'W' && (text[1] == '\0' || text[1] == ' '))
        return 1;
    return 0;
}

/**
 * @brief Render text with unit suffixes (MHz, GHz, W, RPM) at 2/3 font size.
 * @details Walks the string, renders numeric/space segments at @p full_size
 * and recognised unit tokens at 2/3 of @p full_size.  Uses Cairo's
 * current-point advancement so no manual x-tracking is needed.
 * @param cr      Cairo context (font face must already be selected)
 * @param full_size  Font size for numeric parts
 * @param x       Left start position
 * @param y       Baseline position
 * @param text    The combined info string (e.g. "1500 MHz  95W")
 * @param draw_output Render when nonzero, otherwise only measure.
 * @return Total advance width with the same unit sizing as the rendered text.
 */
double circle_paint_text_with_small_units(cairo_t *cr, double full_size,
                                         double x, double y,
                                         const char *text, int draw_output)
{
    if (!cr || !text || text[0] == '\0')
        return 0.0;

    const double unit_size = full_size * (2.0 / 3.0);
    const char *p = text;
    char segment[64];
    double width = 0.0;

    cairo_move_to(cr, x, y);
    cairo_set_font_size(cr, full_size);

    while (*p)
    {
        int unit_len = match_unit_at(p);
        if (unit_len > 0)
        {
            if ((size_t)unit_len >= sizeof(segment))
                unit_len = (int)(sizeof(segment) - 1);
            memcpy(segment, p, (size_t)unit_len);
            segment[unit_len] = '\0';

            cairo_set_font_size(cr, unit_size);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, segment, &ext);
            width += ext.x_advance;
            if (draw_output)
                cairo_show_text(cr, segment);
            cairo_set_font_size(cr, full_size);

            p += unit_len;
        }
        else
        {
            int len = 0;
            while (p[len] && match_unit_at(p + len) == 0)
                len++;
            if ((size_t)len >= sizeof(segment))
                len = (int)(sizeof(segment) - 1);
            memcpy(segment, p, (size_t)len);
            segment[len] = '\0';

            cairo_set_font_size(cr, full_size);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, segment, &ext);
            width += ext.x_advance;
            if (draw_output)
                cairo_show_text(cr, segment);

            p += len;
        }
    }
    return width;
}
