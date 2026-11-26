/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_TTY_H
#define COOLERDASH_TTY_H

#include "display.h"


int paint_terminal_overlay(cairo_t *cr, const Config *config,
                           const monitor_sensor_data_t *data,
                           const ScalingParams *params);

#endif
