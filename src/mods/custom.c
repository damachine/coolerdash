/* SPDX-License-Identifier: MIT */
#include "custom.h"
#include "render.h"

int paint_custom_overlay(cairo_t *cr, const Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params)
{
    const CustomLayout *layout = &config->custom_layout;
    const int index = layout_screen_index(config, layout->screen_count);
    return paint_widget_layout(cr, config, data, params, layout, index, NULL);
}
