/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_CIRCLE_TEXT_H
#define COOLERDASH_CIRCLE_TEXT_H

#include "display.h"

typedef struct
{
    int active;
    char number[16];
    double font_size;
    double percent_font_size;
    double number_width;
    double percent_width;
    double spacing;
    double total_width;
    cairo_text_extents_t number_ext;
    cairo_text_extents_t percent_ext;
} DutyLayout;

int circle_extra_info_text(const Config *config, const monitor_sensor_data_t *data,
                           const char *slot_value, char *buf, size_t buf_size);
int circle_extra_info_line2(const monitor_sensor_data_t *data, const char *slot_value,
                            char *buf, size_t buf_size);
void circle_duty_layout(cairo_t *cr, const Config *config, const ScalingParams *params,
                        const monitor_sensor_data_t *data, const char *slot_value,
                        double max_width, double max_height, double preferred_font_size,
                        DutyLayout *layout);
double circle_paint_text_with_small_units(cairo_t *cr, double full_size, double x, double y,
                                          const char *text, int draw_output);

#endif
