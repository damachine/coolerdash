/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <time.h>

#include "background.h"

/**
 * @brief Convert color component into a 0.0-1.0 range.
 */
double cairo_color_convert(uint8_t color_component)
{
    return color_component / 255.0;
}

/**
 * @brief Set cairo source color from Color structure.
 */
void set_cairo_color(cairo_t *cr, const Color *color)
{
    cairo_set_source_rgb(cr, cairo_color_convert(color->r),
                         cairo_color_convert(color->g),
                         cairo_color_convert(color->b));
}

/**
 * @brief Set cairo source color from Color structure with alpha.
 */
void set_cairo_color_alpha(cairo_t *cr, const Color *color, double alpha)
{
    cairo_set_source_rgba(cr, cairo_color_convert(color->r),
                          cairo_color_convert(color->g),
                          cairo_color_convert(color->b), alpha);
}

/**
 * @brief Create cairo surface and context for display rendering.
 * @details Creates ARGB32 surface with dimensions from config.
 */
cairo_t *create_cairo_context(const struct Config *config,
                              cairo_surface_t **surface)
{
    *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, config->display_width, config->display_height);
    if (!*surface || cairo_surface_status(*surface) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to create cairo surface");
        if (*surface)
            cairo_surface_destroy(*surface);
        *surface = NULL;
        return NULL;
    }

    cairo_t *cr = cairo_create(*surface);
    if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to create cairo context");
        if (cr)
            cairo_destroy(cr);
        cairo_surface_destroy(*surface);
        *surface = NULL;
        return NULL;
    }

    return cr;
}

/**
 * @brief Rotate the complete rendered image into the physical LCD canvas.
 * @details Fit the rotated bounds inside the canvas so no content is cut off.
 * The configured background color fills corners outside the rotated image.
 */
static cairo_status_t write_oriented_surface_png(
    cairo_surface_t *source, const struct Config *config,
    int width, int height, const char *path,
    const monitor_sensor_data_t *ring_data, const ScalingParams *ring_params,
    int split_ring, const char *first_slot, const char *second_slot)
{
    if (!source || !config || !path)
        return CAIRO_STATUS_NULL_POINTER;
    if (width <= 0 || height <= 0 ||
        cairo_surface_get_type(source) != CAIRO_SURFACE_TYPE_IMAGE)
        return CAIRO_STATUS_INVALID_SIZE;

    const int source_width = cairo_image_surface_get_width(source);
    const int source_height = cairo_image_surface_get_height(source);
    if (source_width <= 0 || source_height <= 0)
        return CAIRO_STATUS_INVALID_SIZE;

    if (config->display_rotation == 0 &&
        source_width == width && source_height == height)
        return cairo_surface_write_to_png(source, path);

    cairo_surface_t *target =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_status_t status = cairo_surface_status(target);
    if (status != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(target);
        return status;
    }

    cairo_t *cr = cairo_create(target);
    status = cairo_status(cr);
    if (status == CAIRO_STATUS_SUCCESS)
    {
        set_cairo_color(cr, &config->display_background_color);
        cairo_paint(cr);

        const double radians = config->display_rotation * DISPLAY_M_PI / 180.0;
        double cosine = cos(radians);
        double sine = sin(radians);
        if (fabs(cosine) < 1e-12)
            cosine = 0.0;
        if (fabs(sine) < 1e-12)
            sine = 0.0;
        const double bounds_width =
            fabs(source_width * cosine) + fabs(source_height * sine);
        const double bounds_height =
            fabs(source_width * sine) + fabs(source_height * cosine);
        const double scale = fmin((double)width / bounds_width,
                                  (double)height / bounds_height);

        cairo_translate(cr, width / 2.0, height / 2.0);
        cairo_rotate(cr, radians);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, source,
                                 -source_width / 2.0, -source_height / 2.0);
        cairo_pattern_set_filter(cairo_get_source(cr),
                                 config->display_rotation % 90 == 0
                                     ? CAIRO_FILTER_NEAREST
                                     : CAIRO_FILTER_BILINEAR);
        cairo_paint(cr);
        if (ring_data && ring_params && !ring_params->is_circular)
        {
            /* The ring uses physical display coordinates after content rotation. */
            cairo_identity_matrix(cr);
            if (split_ring)
                draw_split_sensor_ring(cr, config, ring_data, ring_params,
                                       first_slot, second_slot);
            else
                draw_sensor_ring_for_slot(cr, config, ring_data, ring_params,
                                          first_slot);
        }
        status = cairo_status(cr);
    }
    cairo_destroy(cr);
    if (status == CAIRO_STATUS_SUCCESS)
        status = cairo_surface_write_to_png(target, path);
    cairo_surface_destroy(target);
    return status;
}

void paint_display_clock(cairo_t *cr, const Config *config)
{
    if (!config->display_show_clock)
        return;

    char clock[6] = "12:34";
    if (!config->preview_render)
    {
        const time_t now = time(NULL);
        struct tm local;
        if (!localtime_r(&now, &local) || !strftime(clock, sizeof(clock), "%H:%M", &local))
            return;
    }

    const double scale = fmin(config->display_width, config->display_height) / 240.0;
    cairo_save(cr);
    cairo_translate(cr, config->display_width / 2.0, config->display_height * 0.90);
    cairo_scale(cr, scale, scale);
    cairo_new_path(cr);
    draw_rounded_rectangle_path(cr, -26, -10, 52, 20, 6);
    cairo_set_source_rgba(cr, 0.04, 0.05, 0.08, 0.82);
    cairo_fill(cr);

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    cairo_text_extents_t ink;
    cairo_text_extents(cr, clock, &ink);
    cairo_move_to(cr, -ink.x_bearing - ink.width / 2, -ink.y_bearing - ink.height / 2);
    cairo_set_source_rgb(cr, 0.96, 0.97, 1.0);
    cairo_show_text(cr, clock);
    cairo_restore(cr);
}

cairo_status_t write_display_png(cairo_surface_t *surface,
                                 const struct Config *config,
                                 const char *path)
{
    return write_display_png_with_ring(surface, config, NULL, NULL, 0, NULL, NULL, path);
}

cairo_status_t write_display_png_with_ring(cairo_surface_t *surface,
                                           const struct Config *config,
                                           const monitor_sensor_data_t *data,
                                           const ScalingParams *params,
                                           int split_ring,
                                           const char *first_slot,
                                           const char *second_slot,
                                           const char *path)
{
    if (!surface || !config || !path)
        return CAIRO_STATUS_NULL_POINTER;
    if (config->display_show_clock && config->display_width && config->display_height)
    {
        cairo_t *cr = cairo_create(surface);
        paint_display_clock(cr, config);
        const cairo_status_t status = cairo_status(cr);
        cairo_destroy(cr);
        if (status != CAIRO_STATUS_SUCCESS)
            return status;
    }
    return write_oriented_surface_png(surface, config,
                                      config->display_width,
                                      config->display_height, path,
                                      data, params, split_ring,
                                      first_slot, second_slot);
}

int render_rotated_image_to_png(const char *source_path,
                                const struct Config *config,
                                int width, int height, const char *output_path)
{
    if (!source_path || !config || !output_path || width <= 0 || height <= 0)
        return 0;

    /* Animated source formats use their first frame for the shutdown image. */
    cairo_surface_t *source = image_file_load_surface(source_path, 0);
    if (!source)
        return 0;
    const cairo_status_t status = write_oriented_surface_png(
        source, config, width, height, output_path,
        NULL, NULL, 0, NULL, NULL);
    cairo_surface_destroy(source);
    return status == CAIRO_STATUS_SUCCESS;
}
