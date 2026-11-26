/* SPDX-License-Identifier: MIT */
#include <pthread.h>

#include "rings.h"
#include "render.h"

static CustomLayout circle_rings_layout;
static pthread_once_t circle_rings_once = PTHREAD_ONCE_INIT;

static void load_circle_rings_preset(void)
{
    load_layout_preset("circle-rings", &circle_rings_layout);
}

int paint_circle_rings_overlay(cairo_t *cr, const Config *config,
                               const monitor_sensor_data_t *data,
                               const ScalingParams *params)
{
    pthread_once(&circle_rings_once, load_circle_rings_preset);
    if (!circle_rings_layout.screen_count)
        return 0;

    const int index = layout_screen_index(config, circle_rings_layout.screen_count);
    return paint_widget_layout(cr, config, data, params, &circle_rings_layout, index,
                               config->rings_font_face);
}
