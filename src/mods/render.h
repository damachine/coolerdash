/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_RENDER_H
#define COOLERDASH_RENDER_H

#include "layout.h"

typedef int (*OverlayPainter)(cairo_t *, const Config *, const monitor_sensor_data_t *,
                              const ScalingParams *);

typedef struct
{
    const char *first_slot;
    const char *second_slot;
    const char *(*current_slot)(const Config *);
    int split;
} FrameRing;

int render_frame(const Config *config, const monitor_sensor_data_t *data,
                 const ScalingParams *params, OverlayPainter paint, const FrameRing *ring);

int paint_widget_layout(cairo_t *cr, const Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params,
                        const CustomLayout *layout, int index, const char *font_override);
void render_divider(cairo_t *cr, const Config *config, const LayoutBox *box, const Color *color);
#endif
