/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_BAR_H
#define COOLERDASH_BAR_H

#include "layout.h"

typedef enum
{
    BAR_ROUNDED_FILL,
    BAR_CLIPPED_FILL
} BarStyle;

void draw_segment_bar(cairo_t *cr, double x, double y, double width, double height,
                      int segments, double progress, const Color *color);
void draw_temperature_bar(cairo_t *cr, const Config *config, const ScalingParams *params,
                          const LayoutBox *box, int fill_width, const Color *color, BarStyle style);

#endif
