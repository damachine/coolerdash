/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_BACKGROUND_H
#define COOLERDASH_BACKGROUND_H

#include "display.h"

cairo_surface_t *image_file_load_surface(const char *path, int use_cache);
void reset_background_state(void);
void draw_background_only_image(const Config *config);

#endif
