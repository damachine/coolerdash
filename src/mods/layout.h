/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_LAYOUT_H
#define COOLERDASH_LAYOUT_H

#include "display.h"

typedef struct
{
    double x, y, width, height;
} LayoutBox;

int layout_columns(const Config *config, const ScalingParams *params, unsigned occupied,
                    LayoutBox columns[2], LayoutBox *divider);
void layout_stack(const LayoutBox *column, const CustomElement *const widgets[],
                   int count, LayoutBox boxes[]);
int layout_arrange_screen(const Config *config, const ScalingParams *params,
                          const CustomScreen *screen, LayoutBox boxes[CUSTOM_LAYOUT_MAX_ELEMENTS]);

#endif
