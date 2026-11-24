/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Display mode dispatcher and shared rendering utilities.
 * @details Routes to dual, circle, or split rendering modules, provides shared Cairo
 * helpers and sensor slot functions used by all display modes.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "../srv/cc_sensor.h"
#include "circle.h"
#include "display.h"
#include "custom.h"
#include "dual.h"
#include "split.h"
#include "tty.h"

/* Preview requests load their own image. This cache belongs to the render thread. */
static struct
{
    char path[CONFIG_MAX_PATH_LEN];
    cairo_surface_t *surface;
} background_image;

/* Owned by the daemon's render thread. Try native playback once per config. */
static enum
{
    NATIVE_GIF_PENDING,
    NATIVE_GIF_INACTIVE,
    NATIVE_GIF_ACTIVE
} native_gif = NATIVE_GIF_PENDING;

static void reset_background_image(void)
{
    if (background_image.surface)
        cairo_surface_destroy(background_image.surface);
    background_image.surface = NULL;
    background_image.path[0] = '\0';
}

static const char *mime_type_for_format(GdkPixbufFormat *format)
{
    if (!format)
        return NULL;

    char *name = gdk_pixbuf_format_get_name(format);
    if (!name)
        return NULL;

    const char *mime_type = NULL;
    if (strcmp(name, "png") == 0)
        mime_type = "image/png";
    else if (strcmp(name, "gif") == 0)
        mime_type = "image/gif";
    else if (strcmp(name, "jpeg") == 0)
        mime_type = "image/jpeg";
    else if (strcmp(name, "bmp") == 0)
        mime_type = "image/bmp";
    else if (strcmp(name, "tiff") == 0)
        mime_type = "image/tiff";

    g_free(name);
    return mime_type;
}

const char *image_file_mime_type(const char *path)
{
    if (!path || path[0] == '\0')
        return NULL;
    return mime_type_for_format(gdk_pixbuf_get_file_info(path, NULL, NULL));
}

int image_file_is_supported(const char *path)
{
    struct stat info;
    if (!path || path[0] != '/' || strlen(path) >= CONFIG_MAX_PATH_LEN ||
        stat(path, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > 16 * 1024 * 1024)
        return 0;

    int width = 0, height = 0;
    GdkPixbufFormat *format = gdk_pixbuf_get_file_info(path, &width, &height);
    if (!mime_type_for_format(format) || width <= 0 || height <= 0 ||
        (uint64_t)width * (uint64_t)height > 32U * 1024U * 1024U)
        return 0;

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(path, 1, 1, TRUE, NULL);
    if (!pixbuf)
        return 0;
    g_object_unref(pixbuf);
    return 1;
}

static char *png_data_uri(const char *png, size_t png_size)
{
    gchar *encoded = g_base64_encode((const guchar *)png, png_size);
    if (!encoded)
        return NULL;
    const char prefix[] = "data:image/png;base64,";
    const size_t size = sizeof(prefix) + strlen(encoded);
    char *result = malloc(size);
    if (result)
        snprintf(result, size, "%s%s", prefix, encoded);
    g_free(encoded);
    return result;
}

char *rendered_png_data_uri(const char *path)
{
    /* The renderer created this PNG in a private temporary file. Keep its
       original pixel dimensions for the editor's 1:1 view. */
    struct stat info;
    if (stat(path, &info) != 0 || info.st_size <= 0 || info.st_size > 2 * 1024 * 1024)
        return NULL;
    gchar *png;
    gsize size;
    if (!g_file_get_contents(path, &png, &size, NULL))
        return NULL;
    char *uri = png_data_uri(png, size);
    g_free(png);
    return uri;
}

char *image_file_preview_data_uri(const char *path)
{
    struct stat info;
    if (!path || path[0] != '/' || strlen(path) >= CONFIG_MAX_PATH_LEN ||
        stat(path, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > 16 * 1024 * 1024)
        return NULL;

    int width = 0, height = 0;
    GdkPixbufFormat *format = gdk_pixbuf_get_file_info(path, &width, &height);
    if (!mime_type_for_format(format) || width <= 0 || height <= 0 ||
        (uint64_t)width * (uint64_t)height > 32U * 1024U * 1024U)
        return NULL;

    /* Load the thumbnail independently of the daemon's background cache. */
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(path, 480, 480, TRUE, NULL);
    if (!pixbuf)
        return NULL;

    gchar *png = NULL;
    gsize png_size = 0;
    gboolean saved = gdk_pixbuf_save_to_buffer(pixbuf, &png, &png_size,
                                               "png", NULL, NULL);
    g_object_unref(pixbuf);
    if (!saved || png_size > 1024U * 1024U)
    {
        g_free(png);
        return NULL;
    }
    char *result = png_data_uri(png, png_size);
    g_free(png);
    return result;
}

cairo_surface_t *pixbuf_to_cairo_surface(GdkPixbuf *pixbuf)
{
    if (!pixbuf)
        return NULL;

    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);
    const int source_stride = gdk_pixbuf_get_rowstride(pixbuf);
    const int has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    const int bits_per_sample = gdk_pixbuf_get_bits_per_sample(pixbuf);
    const GdkColorspace colorspace = gdk_pixbuf_get_colorspace(pixbuf);
    const guchar *source = gdk_pixbuf_read_pixels(pixbuf);

    if (width <= 0 || height <= 0 || colorspace != GDK_COLORSPACE_RGB ||
        bits_per_sample != 8 || !source ||
        (channels != 3 && channels != 4) || (has_alpha && channels != 4))
    {
        return NULL;
    }

    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(surface);
        return NULL;
    }

    cairo_surface_flush(surface);
    unsigned char *destination = cairo_image_surface_get_data(surface);
    const int destination_stride = cairo_image_surface_get_stride(surface);

    for (int y = 0; y < height; y++)
    {
        const guchar *source_row = source + (size_t)y * (size_t)source_stride;
        uint32_t *destination_row =
            (uint32_t *)(destination + (size_t)y * (size_t)destination_stride);
        for (int x = 0; x < width; x++)
        {
            const guchar *pixel = source_row + (size_t)x * (size_t)channels;
            const uint32_t alpha = has_alpha ? pixel[3] : 255U;
            const uint32_t red = ((uint32_t)pixel[0] * alpha + 127U) / 255U;
            const uint32_t green = ((uint32_t)pixel[1] * alpha + 127U) / 255U;
            const uint32_t blue = ((uint32_t)pixel[2] * alpha + 127U) / 255U;
            destination_row[x] =
                (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }

    cairo_surface_mark_dirty(surface);
    return surface;
}

static cairo_surface_t *image_file_load_surface(const char *path, int use_cache)
{
    if (use_cache)
    {
        if (background_image.surface && strcmp(background_image.path, path) == 0)
            return cairo_surface_reference(background_image.surface);

        reset_background_image();
        if (!image_file_mime_type(path))
            return NULL;
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, NULL);
    if (!pixbuf)
        return NULL;

    cairo_surface_t *surface = pixbuf_to_cairo_surface(pixbuf);
    g_object_unref(pixbuf);

    if (use_cache && surface)
    {
        background_image.surface = cairo_surface_reference(surface);
        SAFE_STRCPY(background_image.path, path);
    }
    return surface;
}

static double clamp_double(double value, double min_value, double max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

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

double fit_text_font_size(cairo_t *cr, const char *text,
                          double preferred_size, double max_width,
                          double max_height, double minimum_size)
{
    if (!cr || !text || max_width <= 0.0 || max_height <= 0.0)
        return fmax(1.0, minimum_size);

    double size = fmax(1.0, preferred_size);
    const double floor_size = fmax(1.0, minimum_size);

    while (size > floor_size)
    {
        cairo_text_extents_t text_ext = {0};
        cairo_set_font_size(cr, size);
        cairo_text_extents(cr, text, &text_ext);

        if (fmax(text_ext.x_advance, text_ext.width) <= max_width &&
            text_ext.height <= max_height)
            return size;

        size *= 0.92;
    }

    /* The lower bound is soft: geometry must remain valid on tiny displays. */
    size = floor_size;
    while (size > 1.0)
    {
        cairo_text_extents_t text_ext = {0};
        cairo_set_font_size(cr, size);
        cairo_text_extents(cr, text, &text_ext);
        if (fmax(text_ext.x_advance, text_ext.width) <= max_width &&
            text_ext.height <= max_height)
            break;
        size *= 0.92;
    }
    return fmax(1.0, size);
}

/**
 * @brief Scale a design-space X value based on display width.
 */
double scale_value_x(const ScalingParams *params, double value)
{
    if (!params)
        return value;
    return value * params->scale_x;
}

/**
 * @brief Scale a design-space Y value based on display height.
 */
double scale_value_y(const ScalingParams *params, double value)
{
    if (!params)
        return value;
    return value * params->scale_y;
}

/**
 * @brief Scale a design-space value using average display scaling.
 */
double scale_value_avg(const ScalingParams *params, double value)
{
    if (!params)
        return value;
    return value * params->scale_uniform;
}

/**
 * @brief Get effective degree symbol spacing for the current display.
 */
int get_scaled_degree_spacing(const struct Config *config,
                              const ScalingParams *params)
{
    const double base_spacing =
        (config && config->display_degree_spacing > 0)
            ? (double)config->display_degree_spacing
            : 16.0;

    const int scaled_spacing = (int)lround(scale_value_avg(params, base_spacing));
    return (scaled_spacing > 0) ? scaled_spacing : 1;
}

/**
 * @brief Get the scaled bar border width.
 */
double get_scaled_bar_border_width(const struct Config *config,
                                   const ScalingParams *params)
{
    if (!config || config->layout_bar_border <= 0.0f)
        return 0.0;

    const double scaled_border = scale_value_avg(params,
                                                 (double)config->layout_bar_border);
    return (scaled_border > 0.0) ? scaled_border : 0.0;
}

/**
 * @brief Get the preferred label font size.
 */
double get_preferred_label_font_size(const struct Config *config,
                                     const ScalingParams *params)
{
    if (!config)
        return 0.0;

    if (config->font_size_labels > 0.0f)
        return scale_value_avg(params, (double)config->font_size_labels);

    /* Auto starts deliberately large; each mode fits it to its own region. */
    return scale_value_avg(params, 400.0);
}

/**
 * @brief Get the scaled global label X offset using uniform scaling.
 */
int get_scaled_label_offset_x(const struct Config *config,
                              const ScalingParams *params)
{
    if (!config)
        return 0;

    return (int)lround(scale_value_avg(
        params, (double)config->display_label_offset_x));
}

/**
 * @brief Get the scaled global label Y offset using uniform scaling.
 */
int get_scaled_label_offset_y(const struct Config *config,
                              const ScalingParams *params)
{
    if (!config)
        return 0;

    return (int)lround(scale_value_avg(
        params, (double)config->display_label_offset_y));
}

/**
 * @brief Get the scaled pixel bar gap.
 */
int get_scaled_bar_gap(const struct Config *config,
                       const ScalingParams *params)
{
    if (!config)
        return 0;

    const int scaled_gap = (int)lround(scale_value_avg(
        params, (double)config->layout_bar_gap));
    return (scaled_gap > 0) ? scaled_gap : 0;
}

/**
 * @brief Get the effective vertical spacing between bars and labels.
 */
double get_effective_label_spacing(const struct Config *config,
                                   const ScalingParams *params)
{
    if (!config)
        return 0.0;

    const double spacing_factor =
        fmax(0.01, config->layout_label_margin_bar / 100.0);
    const double min_dimension =
        (config->display_width < config->display_height)
            ? (double)config->display_width
            : (double)config->display_height;
    const double scaled_spacing = min_dimension * spacing_factor;
    const double minimum_spacing = scale_value_avg(params, 2.0);

    return fmax(minimum_spacing, scaled_spacing);
}

/**
 * @brief Format a sensor value with optional decimals.
 */
void format_slot_value_text(char *buffer, size_t buffer_size,
                            const monitor_sensor_data_t *data,
                            const char *slot_value, float temp_value)
{
    if (!buffer || buffer_size == 0)
        return;

    if (get_slot_use_decimal(data, slot_value))
        snprintf(buffer, buffer_size, "%.1f", temp_value);
    else
        snprintf(buffer, buffer_size, "%d", (int)temp_value);
}

/**
 * @brief Measure and optionally render a right-aligned sensor value block.
 */
void layout_and_render_slot_value(cairo_t *cr,
                                  const monitor_sensor_data_t *data,
                                  const struct Config *config,
                                  const ScalingParams *params,
                                  const char *slot_value,
                                  float temp_value, double box_x,
                                  double box_y, double box_width,
                                  double box_height, int align_bottom,
                                  int draw_output,
                                  SlotValueLayout *layout)
{
    if (!cr || !data || !config || !params || !slot_value || !layout ||
        box_width <= 0.0 || box_height <= 0.0)
        return;

    memset(layout, 0, sizeof(*layout));

    char value_text[16] = {0};
    format_slot_value_text(value_text, sizeof(value_text), data, slot_value,
                           temp_value);

    const int is_temp = get_slot_is_temp(data, slot_value);
    const double offset_x =
        scale_value_x(params, (double)get_slot_offset_x(config, slot_value));
    const double offset_y =
        scale_value_y(params, (double)get_slot_offset_y(config, slot_value));
    const int degree_spacing =
        is_temp ? ((get_scaled_degree_spacing(config, params) > 1)
                       ? (int)lround(get_scaled_degree_spacing(config, params) * 0.65)
                       : 1)
                : get_scaled_degree_spacing(config, params);
    const double temp_margin_factor =
        (config->layout_label_margin_left > 0)
            ? (config->layout_label_margin_left / 200.0)
            : 0.005;
    const double right_margin = fmax(0.0, box_width * temp_margin_factor);
    const double inner_padding_x = clamp_double(scale_value_avg(params, 0.75),
                                                0.0, box_width * 0.015);
    const double inner_padding_y = clamp_double(scale_value_avg(params, 0.25),
                                                0.0, box_height * 0.02);
    const double available_width =
        fmax(1.0, box_width - inner_padding_x - right_margin);
    const double available_height =
        fmax(1.0, box_height - inner_padding_y);
    const double configured_font_size = get_slot_font_size(config, slot_value);
    const double configured_scaled_font_size =
        (configured_font_size > 0.0)
            ? scale_value_avg(params, configured_font_size)
            : 0.0;
    const double auto_font_size = fmax(scale_value_avg(params, 220.0),
                                       available_height * 7.40);
    const double preferred_font_size =
        (configured_scaled_font_size > 0.0)
            ? configured_scaled_font_size
            : auto_font_size;
    cairo_text_extents_t number_ext = {0};
    cairo_text_extents_t degree_ext = {0};
    double font_size = preferred_font_size;
    double degree_font_size = font_size / 2.05;
    double total_width = 0.0;
    double number_width = 0.0;
    double content_height = 0.0;

    while (1)
    {
        cairo_set_font_size(cr, font_size);
        cairo_text_extents(cr, value_text, &number_ext);
        number_width = fmax(number_ext.x_advance, number_ext.width);

        degree_font_size = font_size / 2.05;
        degree_ext = (cairo_text_extents_t){0};
        if (is_temp)
        {
            cairo_set_font_size(cr, degree_font_size);
            cairo_text_extents(cr, "\xC2\xB0", &degree_ext);
            cairo_set_font_size(cr, font_size);
        }

        total_width = number_width +
                      (is_temp
                           ? (degree_spacing +
                              fmax(degree_ext.x_advance, degree_ext.width))
                           : 0.0);
        content_height = number_ext.height;
        if (is_temp)
            content_height = fmax(content_height,
                                  number_ext.height * 0.08 + degree_ext.height);

        if ((total_width <= available_width &&
             content_height <= available_height) ||
            font_size <= 1.0)
            break;

        font_size *= 0.93;
        if (font_size < 1.0)
            font_size = 1.0;
    }

    if (configured_scaled_font_size <= 0.0)
    {
        double growth_step = fmax(0.5, scale_value_avg(params, 1.0));

        while (growth_step >= 0.25)
        {
            while (1)
            {
                const double candidate_font_size = font_size + growth_step;
                cairo_text_extents_t candidate_number_ext = {0};
                cairo_text_extents_t candidate_degree_ext = {0};
                double candidate_number_width = 0.0;
                double candidate_total_width = 0.0;
                double candidate_content_height = 0.0;
                double candidate_degree_font_size = candidate_font_size / 2.05;

                cairo_set_font_size(cr, candidate_font_size);
                cairo_text_extents(cr, value_text, &candidate_number_ext);
                candidate_number_width =
                    fmax(candidate_number_ext.x_advance,
                         candidate_number_ext.width);

                if (is_temp)
                {
                    cairo_set_font_size(cr, candidate_degree_font_size);
                    cairo_text_extents(cr, "\xC2\xB0", &candidate_degree_ext);
                    cairo_set_font_size(cr, candidate_font_size);
                }

                candidate_total_width = candidate_number_width +
                                        (is_temp
                                             ? (degree_spacing +
                                                fmax(candidate_degree_ext.x_advance,
                                                     candidate_degree_ext.width))
                                             : 0.0);
                candidate_content_height = candidate_number_ext.height;
                if (is_temp)
                    candidate_content_height =
                        fmax(candidate_content_height,
                             candidate_number_ext.height * 0.08 +
                                 candidate_degree_ext.height);

                if (candidate_total_width > available_width ||
                    candidate_content_height > available_height)
                    break;

                font_size = candidate_font_size;
                degree_font_size = candidate_degree_font_size;
                number_ext = candidate_number_ext;
                degree_ext = candidate_degree_ext;
                number_width = candidate_number_width;
                total_width = candidate_total_width;
                content_height = candidate_content_height;
            }

            growth_step *= 0.5;
        }
    }

    degree_font_size = font_size / 2.05;
    cairo_set_font_size(cr, font_size);
    cairo_text_extents(cr, value_text, &number_ext);
    number_width = fmax(number_ext.x_advance, number_ext.width);
    degree_ext = (cairo_text_extents_t){0};
    if (is_temp)
    {
        cairo_set_font_size(cr, degree_font_size);
        cairo_text_extents(cr, "\xC2\xB0", &degree_ext);
        cairo_set_font_size(cr, font_size);
    }
    total_width = number_width +
                  (is_temp
                       ? degree_spacing +
                             fmax(degree_ext.x_advance, degree_ext.width)
                       : 0.0);
    content_height = number_ext.height;
    if (is_temp)
        content_height = fmax(content_height,
                              number_ext.height * 0.08 + degree_ext.height);

    const double requested_right =
        box_x + box_width - right_margin + offset_x;
    const double minimum_left = box_x + inner_padding_x;
    const double maximum_left =
        fmax(minimum_left,
             box_x + box_width - right_margin - total_width);
    const double block_left =
        clamp_double(requested_right - total_width,
                     minimum_left, maximum_left);

    double block_top;
    if (align_bottom)
    {
        const double ink_bottom =
            box_y + box_height - inner_padding_y + offset_y;
        block_top = ink_bottom - content_height;
    }
    else
    {
        block_top = box_y + inner_padding_y + offset_y;
    }

    const double minimum_top = box_y + inner_padding_y;
    const double maximum_top =
        fmax(minimum_top,
             box_y + box_height - inner_padding_y - content_height);
    block_top = clamp_double(block_top, minimum_top, maximum_top);

    const double local_baseline = -number_ext.y_bearing;
    const double baseline_y = block_top + local_baseline;

    if (draw_output)
    {
        cairo_set_font_size(cr, font_size);
        cairo_move_to(cr, block_left - number_ext.x_bearing, baseline_y);
        cairo_show_text(cr, value_text);

        if (is_temp)
        {
            const double degree_top = block_top + number_ext.height * 0.08;
            const double degree_x =
                block_left + number_width + degree_spacing - degree_ext.x_bearing;
            const double degree_y = degree_top - degree_ext.y_bearing;

            cairo_set_font_size(cr, degree_font_size);
            cairo_move_to(cr, degree_x, degree_y);
            cairo_show_text(cr, "\xC2\xB0");
            cairo_set_font_size(cr, font_size);
        }
    }

    layout->active = 1;
    layout->block_left = block_left;
    layout->block_right = block_left + total_width;
    layout->block_top = block_top;
    layout->block_bottom = block_top + content_height;
    layout->baseline_y = baseline_y;
    layout->font_size = font_size;
}

/**
 * @brief Calculate usable horizontal bounds for a text lane.
 */
void calculate_text_lane_bounds(const struct Config *config,
                                const ScalingParams *params,
                                double box_y, double box_height,
                                int align_bottom,
                                double fallback_x, double fallback_width,
                                double *safe_x, double *safe_width)
{
    if (!safe_x || !safe_width)
        return;

    *safe_x = fallback_x;
    *safe_width = fallback_width;

    if (!config || !params || !params->is_circular || box_height <= 0.0)
        return;

    const double band_ratio = 0.36;
    const double band_height = box_height * band_ratio;
    const double band_y = align_bottom ? box_y + box_height - band_height : box_y;
    calculate_safe_region_bounds(params, band_y, band_height, 1.0,
                                 fallback_x, fallback_width,
                                 safe_x, safe_width);
}

void calculate_safe_region_bounds(const ScalingParams *params,
                                  double region_y, double region_height,
                                  double width_factor,
                                  double fallback_x, double fallback_width,
                                  double *safe_x, double *safe_width)
{
    if (!safe_x || !safe_width)
        return;

    *safe_x = fallback_x;
    *safe_width = fallback_width;

    if (!params || !params->is_circular || params->circle_radius <= 0.0)
        return;

    calculate_circle_chord_bounds(
        params->circle_center_x, params->circle_center_y,
        params->circle_radius, region_y, region_height, width_factor,
        safe_x, safe_width);
}

static double get_sensor_ring_line_width(const struct Config *config,
                                         const ScalingParams *params,
                                         double reference_radius)
{
    if (config->layout_ring_width > 0.0f)
        return fmax(1.0, scale_value_avg(params, config->layout_ring_width));
    return fmax(2.0, reference_radius * 0.035);
}

void draw_sensor_ring(cairo_t *cr, const struct Config *config,
                      const monitor_sensor_data_t *data,
                      const ScalingParams *params)
{
    if (!config)
        return;

    draw_sensor_ring_for_slot(cr, config, data, params,
                              config->sensor_ring_sensor);
}

void draw_sensor_ring_for_slot(cairo_t *cr, const struct Config *config,
                               const monitor_sensor_data_t *data,
                               const ScalingParams *params,
                               const char *slot_value)
{
    if (!cr || !config || !data || !params ||
        !config->sensor_ring_enabled ||
        !slot_is_active(slot_value))
        return;

    const sensor_entry_t *sensor = find_sensor_for_slot(data, slot_value);
    if (!sensor)
        return;

    const double max_value = fmax(
        1.0, get_slot_max_scale(config, slot_value));
    const double progress = fmax(0.0, fmin(1.0, sensor->value / max_value));
    const double min_dimension = fmin((double)config->display_width,
                                      (double)config->display_height);
    const double content_scale =
        (config->display_content_scale_factor > 0.0f &&
         config->display_content_scale_factor <= 1.0f)
            ? config->display_content_scale_factor
            : 0.98;
    const double reference_radius = params->is_circular
                                        ? params->circle_radius
                                        : min_dimension * 0.5 * content_scale;
    const double line_width =
        get_sensor_ring_line_width(config, params, reference_radius);
    const double opacity = config->layout_ring_opacity;
    const Color active = get_slot_bar_color(config, slot_value, sensor->value);

    cairo_save(cr);
    cairo_set_line_width(cr, line_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (!params->is_circular)
    {
        const double padding_x =
            ((double)config->display_width * (1.0 - content_scale) * 0.5) +
            line_width * 0.75;
        const double padding_y =
            ((double)config->display_height * (1.0 - content_scale) * 0.5) +
            line_width * 0.75;
        const int x = (int)lround(padding_x);
        const int y = (int)lround(padding_y);
        const int width = config->display_width - 2 * x;
        const int height = config->display_height - 2 * y;
        const double short_side = fmin((double)width, (double)height);
        const double corner_radius = config->layout_ring_rounded_corners
            ? fmin(short_side * 0.5,
                   fmax(line_width, short_side *
                       (params->shape == DISPLAY_SHAPE_ROUNDED_SQUARE ? 0.16 : 0.08)))
            : 0.0;

        if (width > 2 && height > 2)
        {
            const double perimeter =
                2.0 * ((double)width + (double)height -
                       4.0 * corner_radius) +
                2.0 * DISPLAY_M_PI * corner_radius;

            set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                                  0.72 * opacity);
            draw_rounded_rectangle_path(cr, x, y, width, height,
                                        corner_radius);
            cairo_stroke(cr);

            if (progress > 0.0)
            {
                set_cairo_color_alpha(cr, &active, opacity);
                if (progress < 1.0 && perimeter > 0.0)
                {
                    const double dashes[2] = {
                        perimeter * progress,
                        perimeter * (1.0 - progress)};
                    /* Move the value arc, not the rectangular outline. */
                    const double offset =
                        -perimeter * config->display_rotation / 360.0;
                    cairo_set_dash(cr, dashes, 2, offset);
                }
                draw_rounded_rectangle_path(cr, x, y, width, height,
                                            corner_radius);
                cairo_stroke(cr);
            }
        }
        cairo_restore(cr);
        return;
    }

    const double radius = fmax(1.0, params->circle_radius - line_width * 0.75);
    const double degrees = DISPLAY_M_PI / 180.0;
    const double start = 135.0 * degrees;
    const double end = start + 270.0 * degrees;
    set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                          0.72 * opacity);
    cairo_new_sub_path(cr);
    cairo_arc(cr, params->circle_center_x, params->circle_center_y,
              radius, start, end);
    cairo_stroke(cr);

    if (progress > 0.0)
    {
        set_cairo_color_alpha(cr, &active, opacity);
        cairo_new_sub_path(cr);
        cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                  radius, start, start + (end - start) * progress);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

void draw_split_sensor_ring(cairo_t *cr, const struct Config *config,
                            const monitor_sensor_data_t *data,
                            const ScalingParams *params,
                            const char *left_slot, const char *right_slot)
{
    if (!cr || !config || !data || !params ||
        !config->split_ring_by_sensor)
        return;

    const sensor_entry_t *left = find_sensor_for_slot(data, left_slot);
    const sensor_entry_t *right = find_sensor_for_slot(data, right_slot);
    if (!left && !right)
        return;

    const double min_dimension = fmin((double)config->display_width,
                                      (double)config->display_height);
    const double content_scale =
        (config->display_content_scale_factor > 0.0f &&
         config->display_content_scale_factor <= 1.0f)
            ? config->display_content_scale_factor
            : 0.98;
    const double reference_radius = params->is_circular
                                        ? params->circle_radius
                                        : min_dimension * 0.5 * content_scale;
    const double line_width =
        get_sensor_ring_line_width(config, params, reference_radius);
    const double opacity = config->layout_ring_opacity;

    cairo_save(cr);
    cairo_set_line_width(cr, line_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (params->is_circular)
    {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        const double radius =
            fmax(1.0, params->circle_radius - line_width * 0.75);
        const double start = 135.0 * DISPLAY_M_PI / 180.0;
        const double middle = start + 135.0 * DISPLAY_M_PI / 180.0;
        const double end = start + 270.0 * DISPLAY_M_PI / 180.0;

        set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                              0.72 * opacity);
        cairo_new_sub_path(cr);
        cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                  radius, start, end);
        cairo_stroke(cr);

        if (left)
        {
            const Color color = get_slot_bar_color(config, left_slot,
                                                   left->value);
            set_cairo_color_alpha(cr, &color, opacity);
            cairo_new_sub_path(cr);
            cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                      radius, start, middle);
            cairo_stroke(cr);
        }
        if (right)
        {
            const Color color = get_slot_bar_color(config, right_slot,
                                                   right->value);
            set_cairo_color_alpha(cr, &color, opacity);
            cairo_new_sub_path(cr);
            cairo_arc(cr, params->circle_center_x, params->circle_center_y,
                      radius, middle, end);
            cairo_stroke(cr);
        }
        cairo_restore(cr);
        return;
    }

    const double padding_x =
        ((double)config->display_width * (1.0 - content_scale) * 0.5) +
        line_width * 0.75;
    const double padding_y =
        ((double)config->display_height * (1.0 - content_scale) * 0.5) +
        line_width * 0.75;
    const int x = (int)lround(padding_x);
    const int y = (int)lround(padding_y);
    const int width = config->display_width - 2 * x;
    const int height = config->display_height - 2 * y;
    if (width <= 2 || height <= 2)
    {
        cairo_restore(cr);
        return;
    }

    const double short_side = fmin((double)width, (double)height);
    const double corner_radius = config->layout_ring_rounded_corners
        ? fmin(short_side * 0.5,
               fmax(line_width, short_side *
                   (params->shape == DISPLAY_SHAPE_ROUNDED_SQUARE ? 0.16 : 0.08)))
        : 0.0;

    set_cairo_color_alpha(cr, &config->layout_bar_color_background,
                          0.72 * opacity);
    draw_rounded_rectangle_path(cr, x, y, width, height, corner_radius);
    cairo_stroke(cr);

    const sensor_entry_t *entries[2] = {left, right};
    const char *slots[2] = {left_slot, right_slot};
    const double perimeter =
        2.0 * ((double)width + (double)height - 4.0 * corner_radius) +
        2.0 * DISPLAY_M_PI * corner_radius;
    const double half = perimeter * 0.5;
    /* The path starts at the top right. The left half starts at bottom center. */
    const double left_start = half - width * 0.5 + corner_radius;
    const double shift = left_start +
                         perimeter * config->display_rotation / 360.0;
    for (int i = 0; i < 2; i++)
    {
        if (!entries[i])
            continue;
        const Color color =
            get_slot_bar_color(config, slots[i], entries[i]->value);
        cairo_save(cr);
        if (config->display_rotation == 0)
        {
            cairo_rectangle(cr, i == 0 ? 0.0 : config->display_width * 0.5,
                            0.0, config->display_width * 0.5,
                            config->display_height);
            cairo_clip(cr);
        }
        else
        {
            const double dashes[2] = {half, half};
            cairo_set_dash(cr, dashes, 2, i == 0 ? -shift : half - shift);
        }
        set_cairo_color_alpha(cr, &color, opacity);
        draw_rounded_rectangle_path(cr, x, y, width, height, corner_radius);
        cairo_stroke(cr);
        cairo_restore(cr);
    }
    cairo_restore(cr);
}

void calculate_bar_bounds(const struct Config *config,
                          const ScalingParams *params,
                          double bar_y, double bar_height,
                          int *bar_x, int *bar_width)
{
    if (!config || !params || !bar_x || !bar_width)
        return;

    double safe_x = params->safe_content_margin;
    double safe_width = params->safe_bar_width;
    const double bar_width_factor = (config->layout_bar_width > 0)
                                        ? config->layout_bar_width / 100.0
                                        : 0.98;

    calculate_safe_region_bounds(params, bar_y, bar_height, bar_width_factor,
                                 safe_x, safe_width, &safe_x, &safe_width);
    *bar_x = (int)lround(safe_x);
    *bar_width = (int)lround(safe_width);
}

void draw_segment_bar(cairo_t *cr, double x, double y, double width, double height,
                      int segments, double progress, const Color *color)
{
    const double step = width / segments;
    const double gap = segments > 1 ? fmin(step * 0.24, height * 0.4) : 0;
    cairo_save(cr);
    for (int i = 0; i < segments; i++)
        cairo_rectangle(cr, x + step * i, y, step - gap, height);
    cairo_clip(cr);
    set_cairo_color_alpha(cr, color, 0.22);
    cairo_paint(cr);
    if (progress > 0)
    {
        set_cairo_color(cr, color);
        cairo_rectangle(cr, x, y, width * progress, height);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

/** Paint a configured overlay over a loaded background image. */
static void paint_background_overlay(cairo_t *cr, const struct Config *config)
{
    if (config->display_background_overlay_opacity <= 0.0f)
        return;

    const Color *background = &config->display_background_color;
    cairo_save(cr);
    cairo_set_source_rgba(cr,
                          cairo_color_convert(background->r),
                          cairo_color_convert(background->g),
                          cairo_color_convert(background->b),
                          config->display_background_overlay_opacity);
    cairo_paint(cr);
    cairo_restore(cr);
}

/**
 * @brief Paint configured background image or fallback color.
 */
void paint_display_background_surface(cairo_t *cr, const struct Config *config,
                                       cairo_surface_t *background)
{
    static char last_failed_path[CONFIG_MAX_PATH_LEN] = {0};

    if (!cr || !config)
        return;

    set_cairo_color(cr, &config->display_background_color);
    cairo_paint(cr);

    if (background)
    {
        if (cairo_surface_status(background) == CAIRO_STATUS_SUCCESS)
        {
            const int image_width = cairo_image_surface_get_width(background);
            const int image_height = cairo_image_surface_get_height(background);

            if (image_width > 0 && image_height > 0)
            {
                double scale_x = 1.0;
                double scale_y = 1.0;
                double offset_x = 0.0;
                double offset_y = 0.0;

                if (strcmp(config->display_background_image_fit, "contain") == 0)
                {
                    const double scale = fmin((double)config->display_width / image_width,
                                              (double)config->display_height / image_height);
                    scale_x = scale;
                    scale_y = scale;
                    offset_x = ((double)config->display_width - image_width * scale) / 2.0;
                    offset_y = ((double)config->display_height - image_height * scale) / 2.0;
                }
                else if (strcmp(config->display_background_image_fit, "stretch") == 0)
                {
                    scale_x = (double)config->display_width / image_width;
                    scale_y = (double)config->display_height / image_height;
                }
                else
                {
                    const double scale = fmax((double)config->display_width / image_width,
                                              (double)config->display_height / image_height);
                    scale_x = scale;
                    scale_y = scale;
                    offset_x = ((double)config->display_width - image_width * scale) / 2.0;
                    offset_y = ((double)config->display_height - image_height * scale) / 2.0;
                }

                const double image_zoom =
                    config->background_image_scale_factor > 0.0f
                        ? config->background_image_scale_factor
                        : 1.0;
                const double center_x = config->display_width / 2.0;
                const double center_y = config->display_height / 2.0;

                cairo_save(cr);
                cairo_translate(cr, center_x, center_y);
                cairo_scale(cr, image_zoom, image_zoom);
                cairo_translate(cr, -center_x, -center_y);
                cairo_translate(cr, offset_x, offset_y);
                cairo_scale(cr, scale_x, scale_y);
                cairo_set_source_surface(cr, background, 0.0, 0.0);
                cairo_pattern_set_filter(cairo_get_source(cr),
                                         CAIRO_FILTER_BILINEAR);
                cairo_paint(cr);
                cairo_restore(cr);

                paint_background_overlay(cr, config);
                if (!config->preview_render)
                    last_failed_path[0] = '\0';
                return;
            }
        }
    }

    if (config->paths_image_background[0] != '\0')
    {
        if (!config->preview_render &&
            strcmp(last_failed_path, config->paths_image_background) != 0)
        {
            SAFE_STRCPY(last_failed_path, config->paths_image_background);
            log_message(LOG_WARNING,
                        "Failed to load background image, using background color: %s",
                        config->paths_image_background);
        }
    }

}

void paint_display_background(cairo_t *cr, const struct Config *config)
{
    if (!cr || !config)
        return;

    cairo_surface_t *image = NULL;
    if (config->paths_image_background[0])
        image = image_file_load_surface(config->paths_image_background,
                                        !config->preview_render);
    paint_display_background_surface(cr, config, image);
    if (image)
        cairo_surface_destroy(image);
}

/**
 * @brief Calculate temperature fill width with bounds checking.
 */
int calculate_temp_fill_width(float temp_value, int max_width, float max_temp)
{
    if (temp_value <= 0.0f)
        return 0;

    const float ratio = fminf(temp_value / max_temp, 1.0f);
    return (int)(ratio * max_width);
}

/**
 * @brief Draw rounded rectangle path for temperature bars.
 */
void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width,
                                 int height, double radius)
{
    if (radius <= 0.0)
    {
        cairo_rectangle(cr, x, y, width, height);
        return;
    }
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -DISPLAY_M_PI_2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0,
              DISPLAY_M_PI_2);
    cairo_arc(cr, x + radius, y + height - radius, radius, DISPLAY_M_PI_2,
              DISPLAY_M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, DISPLAY_M_PI,
              1.5 * DISPLAY_M_PI);
    cairo_close_path(cr);
}

/**
 * @brief Draw degree symbol at calculated position with proper font scaling.
 */
void draw_degree_symbol(cairo_t *cr, double x, double y,
                        const struct Config *config)
{
    if (!cr || !config)
        return;
    cairo_set_font_size(cr, config->font_size_temp / 1.66);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, "°");
    cairo_set_font_size(cr, config->font_size_temp);
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

    struct stat info;
    int source_width = 0;
    int source_height = 0;
    if (stat(source_path, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > 16 * 1024 * 1024 ||
        !image_file_mime_type(source_path) ||
        !gdk_pixbuf_get_file_info(source_path, &source_width, &source_height) ||
        source_width <= 0 || source_height <= 0 ||
        (uint64_t)source_width * (uint64_t)source_height >
            32U * 1024U * 1024U)
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

/**
 * @brief Calculate dynamic scaling parameters based on display dimensions.
 * @details Known devices use profiles; unknown devices retain the legacy
 * resolution fallback.
 */
void calculate_scaling_params(const struct Config *config,
                              ScalingParams *params, const char *device_name)
{
    const double base_width = 240.0;
    const double base_height = 240.0;
    const double min_dimension =
        (config->display_width < config->display_height)
            ? (double)config->display_width
            : (double)config->display_height;
    const double base_min_dimension =
        (base_width < base_height) ? base_width : base_height;

    params->scale_x = config->display_width / base_width;
    params->scale_y = config->display_height / base_height;
    params->scale_uniform =
        (base_min_dimension > 0.0) ? (min_dimension / base_min_dimension) : 1.0;

    const DisplayProfile *profile = resolve_display_profile(
        0, 0, device_name, config->display_width, config->display_height);
    const int rectangular_canvas = !display_uses_sensor_slots(config) &&
                                   config->display_width != config->display_height;
    params->shape = profile ? profile->shape
                            : (!rectangular_canvas && is_circular_display_device(
                                   device_name, config->display_width,
                                   config->display_height)
                                   ? DISPLAY_SHAPE_CIRCULAR
                                   : DISPLAY_SHAPE_RECTANGULAR);
    params->is_circular = params->shape == DISPLAY_SHAPE_CIRCULAR;
    params->profile_name = profile ? profile->name : "legacy-fallback";

    const float content_scale = (config->display_content_scale_factor > 0.0f &&
                                 config->display_content_scale_factor <= 1.0f)
                                    ? config->display_content_scale_factor
                                    : 0.98f; // Fallback: 98%
    const double center_x_ratio = profile ? profile->center_x_ratio : 0.5;
    const double center_y_ratio = profile ? profile->center_y_ratio : 0.5;
    const double diameter_ratio = profile ? profile->visible_diameter_ratio : 1.0;
    params->circle_center_x = config->display_width * center_x_ratio;
    params->circle_center_y = config->display_height * center_y_ratio;
    params->circle_radius = params->is_circular
                                ? min_dimension * 0.5 * diameter_ratio * content_scale
                                : 0.0;
    const double safe_area_width = params->is_circular
                                       ? params->circle_radius * 2.0
                                       : (double)config->display_width * content_scale;
    const double bar_width_factor = (config->layout_bar_width > 0)
                                        ? (config->layout_bar_width / 100.0)
                                        : 0.98;
    params->safe_bar_width = (int)(safe_area_width * bar_width_factor);
    params->safe_content_margin =
        (config->display_width - params->safe_bar_width) / 2.0;

    params->corner_radius = 8.0 * params->scale_uniform;

    // Calculate vertical margins (0 = auto-detect based on display shape)
    if (config->display_margin_top != 0)
        params->margin_top = config->display_margin_top * params->scale_uniform;
    else
        params->margin_top = params->is_circular
                                 ? (config->display_height * 0.03)
                                 : 0.0;

    if (config->display_margin_bottom != 0)
        params->margin_bottom = config->display_margin_bottom * params->scale_uniform;
    else
        params->margin_bottom = params->is_circular
                                    ? (config->display_height * 0.03)
                                    : 0.0;

    // Log detailed scaling calculations
    log_message(
        LOG_INFO,
        "Scaling: display=%ux%u scale=(%.3f, %.3f) uniform=%.3f "
        "profile=%s shape=%s content=%.3f radius=%.1fpx "
        "center=(%.1f,%.1f) center_bar=%dpx (%.0f%%) margin=%.1fpx "
        "margin_v=(%.1f, %.1f)",
        config->display_width, config->display_height, params->scale_x,
        params->scale_y, params->scale_uniform, params->profile_name,
        display_shape_name(params->shape), content_scale,
        params->circle_radius, params->circle_center_x,
        params->circle_center_y,
        params->safe_bar_width, bar_width_factor * 100.0,
        params->safe_content_margin,
        params->margin_top, params->margin_bottom);
}

int calculate_layout_context(const struct Config *config,
                             const ScalingParams *params,
                             LayoutContext *layout)
{
    if (!config || !params || !layout || config->display_width == 0 ||
        config->display_height == 0)
        return 0;

    memset(layout, 0, sizeof(*layout));
    if (params->is_circular && params->circle_radius > 0.0)
    {
        layout->left = fmax(0.0, params->circle_center_x - params->circle_radius);
        layout->right = fmin((double)config->display_width,
                             params->circle_center_x + params->circle_radius);
        layout->top = fmax(params->margin_top,
                           params->circle_center_y - params->circle_radius);
        layout->bottom = fmin((double)config->display_height - params->margin_bottom,
                              params->circle_center_y + params->circle_radius);
        layout->center_x = params->circle_center_x;
        layout->center_y = params->circle_center_y;
    }
    else
    {
        layout->left = params->safe_content_margin;
        layout->right = config->display_width - params->safe_content_margin;
        layout->top = params->margin_top;
        layout->bottom = config->display_height - params->margin_bottom;
        layout->center_x = config->display_width / 2.0;
        layout->center_y = config->display_height / 2.0;
    }

    layout->width = layout->right - layout->left;
    layout->height = layout->bottom - layout->top;
    return layout->width > 0.0 && layout->height > 0.0;
}

/**
 * @brief Check if a sensor slot is active.
 */
int slot_is_active(const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0')
        return 0;
    return strcmp(slot_value, "none") != 0;
}

int display_has_active_sensor_slots(const struct Config *config)
{
    return config &&
           (slot_is_active(config->sensor_slot_1) ||
            slot_is_active(config->sensor_slot_2) ||
            slot_is_active(config->sensor_slot_3));
}

/**
 * @brief Get sensor value for a slot.
 */
float get_slot_temperature(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 0.0f;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->value;

    return 0.0f;
}

/**
 * @brief Get display label for a sensor slot.
 * @details Returns custom label from SensorConfig if set, otherwise
 * uses legacy names or sensor name from data.
 */
const char *get_slot_label(const struct Config *config,
                           const monitor_sensor_data_t *data,
                           const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0' || strcmp(slot_value, "none") == 0)
        return NULL;

    /* Check for custom label override in SensorConfig */
    if (config)
    {
        const SensorConfig *sc = get_sensor_config(config, slot_value);
        if (sc && sc->label[0] != '\0')
            return sc->label;
    }

    /* Use device name from API if available (first two words only) */
    if (data)
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
        if (entry && entry->device_name[0] != '\0')
        {
            static char short_name[SENSOR_DEVICE_NAME_LEN];
            cc_safe_strcpy(short_name, sizeof(short_name),
                           entry->device_name);
            /* Find end of second word */
            int words = 0;
            for (size_t i = 0; short_name[i] != '\0'; i++)
            {
                if (short_name[i] == ' ')
                {
                    words++;
                    if (words >= 2)
                    {
                        short_name[i] = '\0';
                        break;
                    }
                }
            }
            return short_name;
        }
    }

    /* Fallback: legacy labels */
    if (strcmp(slot_value, "cpu") == 0)
        return "CPU";
    if (strcmp(slot_value, "gpu") == 0)
        return "GPU";
    if (strcmp(slot_value, "liquid") == 0)
        return "LIQ";

    /* Dynamic: use sensor name */
    if (data)
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
        if (entry)
            return entry->name;
    }

    return "???";
}

/**
 * @brief Get bar color for a sensor slot based on value.
 * @details Uses SensorConfig thresholds via get_sensor_config().
 */
Color get_slot_bar_color(const struct Config *config, const char *slot_value,
                         float value)
{
    Color default_color = {0, 255, 0, 1};

    if (!config || !slot_value)
        return default_color;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (!sc)
        return default_color;

    if (value < sc->threshold_1)
        return sc->threshold_1_bar;
    else if (value < sc->threshold_2)
        return sc->threshold_2_bar;
    else if (value < sc->threshold_3)
        return sc->threshold_3_bar;
    else
        return sc->threshold_4_bar;
}

/**
 * @brief Get maximum scale for a sensor slot.
 */
float get_slot_max_scale(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 115.0f;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (sc && sc->max_scale > 0.0f)
        return sc->max_scale;

    return 115.0f;
}

/**
 * @brief Get bar height for a specific slot.
 */
uint16_t get_slot_bar_height(const struct Config *config, const char *slot_name)
{
    if (!config || !slot_name)
        return 20;

    if (strcmp(slot_name, "1") == 0)
    {
        return (config->layout_bar_height_1 > 0)
                   ? config->layout_bar_height_1
                   : config->layout_bar_height;
    }
    else if (strcmp(slot_name, "2") == 0)
    {
        return (config->layout_bar_height_2 > 0)
                   ? config->layout_bar_height_2
                   : config->layout_bar_height;
    }
    else if (strcmp(slot_name, "3") == 0)
    {
        return (config->layout_bar_height_3 > 0)
                   ? config->layout_bar_height_3
                   : config->layout_bar_height;
    }

    return config->layout_bar_height;
}

/**
 * @brief Get the scaled pixel bar height for a slot.
 */
int get_scaled_slot_bar_height(const struct Config *config,
                               const ScalingParams *params,
                               const char *slot_name)
{
    const uint16_t logical_height = get_slot_bar_height(config, slot_name);
    const int scaled_height =
        (int)lround(scale_value_avg(params, (double)logical_height));

    return (scaled_height > 0) ? scaled_height : 1;
}

/**
 * @brief Get display unit string for a sensor slot.
 */
const char *get_slot_unit(const monitor_sensor_data_t *data,
                          const char *slot_value)
{
    if (!data || !slot_value)
        return "\xC2\xB0"
               "C";

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->unit;

    return "\xC2\xB0"
           "C";
}

/**
 * @brief Check if sensor should display decimal values.
 */
int get_slot_use_decimal(const monitor_sensor_data_t *data,
                         const char *slot_value)
{
    if (!data || !slot_value)
        return 0;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->use_decimal;

    return 0;
}

/**
 * @brief Get display X offset for a sensor slot.
 */
int get_slot_offset_x(const struct Config *config, const char *slot_value)
{
    if (!config || !slot_value)
        return 0;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    return sc ? sc->offset_x : 0;
}

/**
 * @brief Get display Y offset for a sensor slot.
 */
int get_slot_offset_y(const struct Config *config, const char *slot_value)
{
    if (!config || !slot_value)
        return 0;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    return sc ? sc->offset_y : 0;
}

/**
 * @brief Check if a sensor slot is a temperature sensor.
 */
int get_slot_is_temp(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 1; /* Default to temp */

    /* Legacy slots are always temperature */
    if (strcmp(slot_value, "cpu") == 0 || strcmp(slot_value, "gpu") == 0 ||
        strcmp(slot_value, "liquid") == 0)
        return 1;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return (entry->category == SENSOR_CATEGORY_TEMP) ? 1 : 0;

    return 1;
}

/**
 * @brief Get font size for a sensor slot.
 * @details Returns per-sensor font size if configured (> 0), otherwise global.
 */
float get_slot_font_size(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 100.0f;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (sc && sc->font_size_temp > 0.0f)
        return sc->font_size_temp;

    return config->font_size_temp;
}

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details Examines display_mode configuration and dispatches to dual, circle,
 * or split renderer. This provides a clean separation between mode selection
 * logic and mode-specific rendering.
 * @param config Configuration containing display mode and rendering parameters
 */
void reset_display_state(void)
{
    native_gif = NATIVE_GIF_PENDING;
    reset_background_image();
    reset_circle_state();
}

void restore_native_gif_after_test(void)
{
    if (native_gif == NATIVE_GIF_ACTIVE)
        native_gif = NATIVE_GIF_PENDING;
}

static int try_native_background_gif(const struct Config *config,
                                      const char *device_uid)
{
    /* CoolerControl fits the GIF to cover the screen and rotates in 90° steps. */
    const Color *background = &config->display_background_color;
    if (config->display_rotation % 90 != 0 ||
        strcmp(config->display_background_image_fit, "cover") != 0 ||
        (config->background_image_scale_factor > 0.0f &&
         config->background_image_scale_factor != 1.0f) ||
        config->display_background_overlay_opacity > 0.0f ||
        background->r || background->g || background->b ||
        !lcd_allows_native_gif(config))
        return 0;

    const char *path = config->paths_image_background;
    if (!image_file_is_supported(path))
        return 0;
    const char *mime_type = image_file_mime_type(path);
    if (!mime_type || strcmp(mime_type, "image/gif") != 0)
        return 0;
    if (!send_gif_to_lcd(config, path, device_uid))
    {
        log_message(LOG_WARNING, "Showing a still image after native GIF upload failed");
        return 0;
    }

    reset_background_image();
    log_message(LOG_STATUS, "Native GIF applied through CoolerControl");
    return 1;
}

static void draw_background_only_image(const struct Config *config)
{
    if (native_gif == NATIVE_GIF_ACTIVE)
        return;

    char device_uid[CC_UID_SIZE] = {0};
    const int device_available =
        is_session_initialized() &&
        get_cached_lcd_device_data(config, device_uid, sizeof(device_uid),
                                   NULL, 0, NULL, NULL) &&
        device_uid[0] != '\0';

    if (device_available && native_gif == NATIVE_GIF_PENDING && !config->display_show_clock)
    {
        native_gif = try_native_background_gif(config, device_uid)
                         ? NATIVE_GIF_ACTIVE
                         : NATIVE_GIF_INACTIVE;
        if (native_gif == NATIVE_GIF_ACTIVE)
            return;
    }

    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return;

    paint_display_background(cr, config);
    cairo_surface_flush(surface);
    const cairo_status_t write_status =
        write_display_png(surface, config, config->paths_image_coolerdash);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    if (write_status != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to write background-only image: %s",
                    cairo_status_to_string(write_status));
        return;
    }

    if (device_available)
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);
}

static int render_overlay_frame(const Config *config, const monitor_sensor_data_t *data,
                                const char *device_name)
{
    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);
    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;
    paint_display_background(cr, config);
    int success = paint_display_overlay(cr, config, data, &params) &&
                  write_display_png(surface, config, config->paths_image_coolerdash) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return success;
}

static void draw_overlay_image(const Config *config)
{
    monitor_sensor_data_t data = {0};
    if (display_needs_sensor_data(config) && !get_sensor_monitor_data(config, &data))
        data.sensor_count = 0;

    char device_uid[CC_UID_SIZE] = {0};
    char device_name[CONFIG_MAX_STRING_LEN] = {0};
    int width = 0;
    int height = 0;
    int available = get_cached_lcd_device_data(config, device_uid, sizeof(device_uid),
                                               device_name, sizeof(device_name), &width, &height);
    if (!render_overlay_frame(config, &data, device_name))
        log_message(LOG_ERROR, "Display rendering failed in %s mode", config->display_mode);
    else if (available && is_session_initialized() && device_uid[0])
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);
}

typedef struct
{
    const char *name;
    int uses_sensor_slots;
    void (*draw)(const Config *);
    int (*preview)(const Config *, const monitor_sensor_data_t *, const char *);
    int (*paint)(cairo_t *, const Config *, const monitor_sensor_data_t *,
                 const ScalingParams *);
} DisplayMode;

static const DisplayMode *get_display_mode(const Config *config)
{
    static const DisplayMode modes[] = {
        {"dual", 1, draw_dual_image, render_dual_preview, paint_dual_overlay},
        {"circle", 1, draw_circle_image, render_circle_preview, paint_circle_overlay},
        {"split", 1, draw_split_image, render_split_preview, paint_split_overlay},
        {"custom", 0, draw_overlay_image, render_overlay_frame, paint_custom_overlay},
        {"terminal", 0, draw_overlay_image, render_overlay_frame, paint_terminal_overlay},
    };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        if (strcmp(config->display_mode, modes[i].name) == 0)
            return &modes[i];
    }
    return &modes[0];
}

int display_uses_sensor_slots(const Config *config)
{
    return get_display_mode(config)->uses_sensor_slots;
}

int display_needs_sensor_data(const Config *config)
{
    if (display_uses_sensor_slots(config))
        return display_has_active_sensor_slots(config);
    return strcmp(config->display_mode, "custom") != 0 || config->custom_layout.needs_sensors;
}

int paint_display_overlay(cairo_t *cr, const struct Config *config,
                           const monitor_sensor_data_t *data,
                           const ScalingParams *params)
{
    return get_display_mode(config)->paint(cr, config, data, params);
}

void draw_display_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config parameter for draw_display_image");
        return;
    }

    if (display_uses_sensor_slots(config) && !display_has_active_sensor_slots(config))
    {
        draw_background_only_image(config);
        return;
    }

    get_display_mode(config)->draw(config);
}

static void add_preview_entry(monitor_sensor_data_t *data,
                              const char *slot_value,
                              sensor_category_t category, float value,
                              const char *unit)
{
    if (!data || !slot_is_active(slot_value) ||
        data->sensor_count >= MAX_SENSORS)
        return;

    sensor_entry_t *entry = &data->sensors[data->sensor_count++];
    const char *separator = strchr(slot_value, ':');
    if (separator && separator != slot_value)
    {
        size_t uid_length = (size_t)(separator - slot_value);
        if (uid_length >= sizeof(entry->device_uid))
            uid_length = sizeof(entry->device_uid) - 1;
        memcpy(entry->device_uid, slot_value, uid_length);
        entry->device_uid[uid_length] = '\0';
        cc_safe_strcpy(entry->name, sizeof(entry->name), separator + 1);
        cc_safe_strcpy(entry->device_name, sizeof(entry->device_name),
                       separator + 1);
        cc_safe_strcpy(entry->device_type, sizeof(entry->device_type), "Custom");
    }
    else
    {
        const char *device_type = strcmp(slot_value, "cpu") == 0
                                      ? "CPU"
                                  : strcmp(slot_value, "gpu") == 0
                                      ? "GPU"
                                      : "Liquidctl";
        cc_safe_strcpy(entry->name, sizeof(entry->name), slot_value);
        cc_safe_strcpy(entry->device_uid, sizeof(entry->device_uid), slot_value);
        cc_safe_strcpy(entry->device_name, sizeof(entry->device_name),
                       get_slot_label(NULL, NULL, slot_value));
        cc_safe_strcpy(entry->device_type, sizeof(entry->device_type), device_type);
    }
    cc_safe_strcpy(entry->unit, sizeof(entry->unit), unit);
    entry->category = category;
    entry->value = value;
    entry->use_decimal = category == SENSOR_CATEGORY_TEMP &&
                         strcmp(slot_value, "liquid") == 0;
}

static void add_preview_sensor(monitor_sensor_data_t *data,
                               const char *slot_value, float temperature)
{
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_TEMP, temperature,
                      "\xC2\xB0\x43");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_DUTY, 54.0f, "%");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_RPM, 1450.0f, "RPM");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_WATTS, 125.0f, "W");
    add_preview_entry(data, slot_value, SENSOR_CATEGORY_FREQ, 4200.0f, "MHz");
}

static void add_processor_preview_channels(monitor_sensor_data_t *data)
{
    add_preview_entry(data, "cpu", SENSOR_CATEGORY_DUTY, 42, "%");
    add_preview_entry(data, "gpu", SENSOR_CATEGORY_DUTY, 66, "%");
    add_preview_entry(data, "cpu", SENSOR_CATEGORY_WATTS, 48, "W");
    add_preview_entry(data, "gpu", SENSOR_CATEGORY_WATTS, 134, "W");
}

static void add_custom_preview_sensors(monitor_sensor_data_t *data, const CustomLayout *layout)
{
    static const struct
    {
        const char *suffix;
        sensor_category_t category;
        float value;
        const char *unit;
    } samples[] = {
        {" RPM", SENSOR_CATEGORY_RPM, 1450, "RPM"},
        {" Duty", SENSOR_CATEGORY_DUTY, 54, "%"},
        {" Watts", SENSOR_CATEGORY_WATTS, 125, "W"},
        {" Freq", SENSOR_CATEGORY_FREQ, 4200, "MHz"},
    };
    int needs_processor_channels = 0;
    for (int i = 0; i < layout->count; i++)
    {
        const char *source = layout->elements[i].source;
        const CustomSource kind = get_custom_source(source);
        if (kind != CUSTOM_SOURCE_SENSOR)
        {
            if (kind != CUSTOM_SOURCE_RAM_USED && kind != CUSTOM_SOURCE_RAM_LOAD)
                needs_processor_channels = 1;
            continue;
        }
        if (!source[0] || find_sensor_for_slot(data, source))
            continue;
        size_t length = strlen(source);
        size_t sample = 0;
        while (sample < sizeof(samples) / sizeof(samples[0]))
        {
            size_t suffix = strlen(samples[sample].suffix);
            if (length > suffix && strcmp(source + length - suffix, samples[sample].suffix) == 0)
                break;
            sample++;
        }
        if (sample < sizeof(samples) / sizeof(samples[0]))
            add_preview_entry(data, source, samples[sample].category,
                              samples[sample].value, samples[sample].unit);
        else
            add_preview_entry(data, source, SENSOR_CATEGORY_TEMP,
                              strcmp(source, "liquid") == 0 ? 32.5f :
                              strcmp(source, "gpu") == 0 ? 62.0f : 45.0f, "°C");
    }
    if (needs_processor_channels)
        add_processor_preview_channels(data);
}

int render_display_preview(const struct Config *config,
                           const char *device_name)
{
    if (!config)
        return 0;

    monitor_sensor_data_t data = {0};
    if (strcmp(config->display_mode, "terminal") == 0)
    {
        add_preview_entry(&data, "cpu", SENSOR_CATEGORY_TEMP, 54, "°C");
        add_preview_entry(&data, "gpu", SENSOR_CATEGORY_TEMP, 62, "°C");
        add_preview_entry(&data, "liquid", SENSOR_CATEGORY_TEMP, 32.5f, "°C");
        add_processor_preview_channels(&data);
    }
    else if (strcmp(config->display_mode, "custom") == 0)
        add_custom_preview_sensors(&data, &config->custom_layout);
    else
    {
        add_preview_sensor(&data, config->sensor_slot_1, 45.0f);
        add_preview_sensor(&data, config->sensor_slot_2, 62.0f);
        add_preview_sensor(&data, config->sensor_slot_3, 32.5f);
        add_preview_sensor(&data, config->sensor_ring_sensor, 32.5f);
    }

    return get_display_mode(config)->preview(config, &data, device_name);
}
