/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "background.h"

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

static GdkPixbuf *load_checked_image(const char *path, int preview_size)
{
    if (!path || path[0] != '/' || strlen(path) >= CONFIG_MAX_PATH_LEN)
        return NULL;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return NULL;

    GdkPixbuf *pixbuf = NULL;
    struct stat info;
    if (fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
        info.st_size > 0 && info.st_size <= DISPLAY_IMAGE_MAX_BYTES)
    {
        /* Inspect and decode the same opened file even if its name is replaced. */
        char fd_path[32];
        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
        int width = 0, height = 0;
        GdkPixbufFormat *format = gdk_pixbuf_get_file_info(fd_path, &width, &height);
        if (mime_type_for_format(format) && width > 0 && height > 0 &&
            (uint64_t)width * (uint64_t)height <= 32U * 1024U * 1024U)
        {
            pixbuf = preview_size > 0 ?
                gdk_pixbuf_new_from_file_at_scale(fd_path, preview_size, preview_size, TRUE, NULL) :
                gdk_pixbuf_new_from_file(fd_path, NULL);
        }
    }
    close(fd);
    return pixbuf;
}

int image_file_is_supported(const char *path)
{
    GdkPixbuf *pixbuf = load_checked_image(path, 1);
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
    GdkPixbuf *pixbuf = load_checked_image(path, 480);
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

cairo_surface_t *image_file_load_surface(const char *path, int use_cache)
{
    if (use_cache)
    {
        if (background_image.surface && strcmp(background_image.path, path) == 0)
            return cairo_surface_reference(background_image.surface);

        reset_background_image();
        if (!image_file_mime_type(path))
            return NULL;
    }

    GdkPixbuf *pixbuf = use_cache ? gdk_pixbuf_new_from_file(path, NULL) : load_checked_image(path, 0);
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

void reset_background_state(void)
{
    native_gif = NATIVE_GIF_PENDING;
    reset_background_image();
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

    log_layout_box(cr, config, "canvas", 0, 0, config->display_width, config->display_height);
    cairo_surface_t *image = NULL;
    if (config->paths_image_background[0])
        image = image_file_load_surface(config->paths_image_background,
                                        !config->preview_render);
    paint_display_background_surface(cr, config, image);
    if (image)
        cairo_surface_destroy(image);
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

void draw_background_only_image(const struct Config *config)
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
