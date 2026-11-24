/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "stream.h"

#include "../device/lcd.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "display.h"
#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static struct
{
    Config config;
    const DisplayProfile *profile;
    StreamLcd *lcd;
    GdkPixbufAnimationIter *gif;
    cairo_surface_t *background;
    cairo_surface_t *overlay;
    cairo_surface_t *frame;
    cairo_t *cr;
    ScalingParams scaling;
    pthread_t sensor_thread;
    int sensor_thread_started;
    int sensor_stop;
    int overlay_dirty;
    gint64 next_frame;
    gint64 next_overlay;
    monitor_sensor_data_t sensors;
} animation;

/* Keep the selected output after a USB failure until config reload. */
static int selected;
static pthread_mutex_t sensor_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sensor_changed = PTHREAD_COND_INITIALIZER;

/* Check the encoded GIF before passing it to GdkPixbuf. Limit both the input
 * size and the total decoded canvas area over all frames. */
static int bounded_gif(const uint8_t *data, size_t size)
{
    if (size < 13 ||
        (memcmp(data, "GIF87a", 6) != 0 && memcmp(data, "GIF89a", 6) != 0))
        return 0;
    const unsigned width = data[6] | (data[7] << 8);
    const unsigned height = data[8] | (data[9] << 8);
    if (!width || !height || width > 2048 || height > 2048)
        return 0;
    const unsigned frame_limit = (64U * 1024U * 1024U) / (4U * width * height);
    size_t pos = 13;
    unsigned frames = 0;
    if (data[10] & 0x80)
        pos += 3U << ((data[10] & 7) + 1);

    while (pos < size)
    {
        const uint8_t marker = data[pos++];
        if (marker == 0x3b)
            return frames > 1;
        if (marker == 0x21 && pos < size)
            pos++; /* extension type */
        else if (marker == 0x2c && size - pos >= 9)
        {
            const unsigned x = data[pos] | (data[pos + 1] << 8);
            const unsigned y = data[pos + 2] | (data[pos + 3] << 8);
            const unsigned w = data[pos + 4] | (data[pos + 5] << 8);
            const unsigned h = data[pos + 6] | (data[pos + 7] << 8);
            if (++frames > frame_limit || !w || !h ||
                x + w > width || y + h > height)
                return 0;
            const uint8_t flags = data[pos + 8];
            pos += 10; /* descriptor and minimum LZW code size */
            if (flags & 0x80)
                pos += 3U << ((flags & 7) + 1);
        }
        else
            return 0;

        while (pos < size)
        {
            const size_t part = data[pos++];
            if (!part)
                break;
            pos += part;
        }
    }
    return 0;
}

/* GdkPixbuf 2.44 deprecates the animation interface. Keep it confined to this
 * optional decoder. The device session and the sensor loop use monotonic time. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GTimeVal gif_time(void)
{
    const gint64 now = g_get_monotonic_time();
    return (GTimeVal){now / G_USEC_PER_SEC, now % G_USEC_PER_SEC};
}

static GdkPixbufAnimationIter *open_gif(const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return NULL;
    FILE *file = fdopen(fd, "rb");
    if (!file)
    {
        close(fd);
        return NULL;
    }
    struct stat info;
    uint8_t *data = NULL;
    if (fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
        info.st_size > 0 && info.st_size <= 16 * 1024 * 1024)
        data = malloc((size_t)info.st_size);
    const size_t size = data ? fread(data, 1, (size_t)info.st_size, file) : 0;
    fclose(file);

    GdkPixbufAnimationIter *iter = NULL;
    if (data && size == (size_t)info.st_size && bounded_gif(data, size))
    {
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new_with_type("gif", NULL);
        if (loader)
        {
            const int loaded = gdk_pixbuf_loader_write(loader, data, size, NULL);
            if (gdk_pixbuf_loader_close(loader, NULL) && loaded)
            {
                GdkPixbufAnimation *gif = gdk_pixbuf_loader_get_animation(loader);
                GTimeVal now = gif_time();
                if (gif && !gdk_pixbuf_animation_is_static_image(gif))
                    iter = gdk_pixbuf_animation_get_iter(gif, &now);
            }
            g_object_unref(loader);
        }
    }
    free(data);
    return iter;
}

static int update_gif(void)
{
    GTimeVal now = gif_time();
    if (!gdk_pixbuf_animation_iter_advance(animation.gif, &now) &&
        animation.background)
        return 1;

    cairo_surface_t *image =
        pixbuf_to_cairo_surface(gdk_pixbuf_animation_iter_get_pixbuf(animation.gif));
    if (!image)
        return 0;
    if (!animation.background)
        animation.background = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, animation.config.display_width,
            animation.config.display_height);
    cairo_t *cr = cairo_create(animation.background);
    paint_display_background_surface(cr, &animation.config, image);
    cairo_status_t status = cairo_status(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(image);
    return status == CAIRO_STATUS_SUCCESS;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void *poll_sensors(void *arg)
{
    const Config *config = arg;
    const int uses_slots = display_uses_sensor_slots(config);
    pthread_mutex_lock(&sensor_lock);
    while (!animation.sensor_stop)
    {
        pthread_mutex_unlock(&sensor_lock);
        monitor_sensor_data_t sensors = {0};
        const int success = get_sensor_monitor_data(config, &sensors);
        pthread_mutex_lock(&sensor_lock);
        if (success || !uses_slots)
        {
            if (!success)
                sensors.sensor_count = 0;
            animation.sensors = sensors;
            animation.overlay_dirty = 1;
        }
        struct timespec next;
        if (clock_gettime(CLOCK_REALTIME, &next) != 0)
            break;
        next.tv_sec++;
        if (!animation.sensor_stop)
            pthread_cond_timedwait(&sensor_changed, &sensor_lock, &next);
    }
    pthread_mutex_unlock(&sensor_lock);
    return NULL;
}

static int update_overlay(gint64 now)
{
    pthread_mutex_lock(&sensor_lock);
    if (!animation.overlay_dirty && now < animation.next_overlay)
    {
        pthread_mutex_unlock(&sensor_lock);
        return 1;
    }
    monitor_sensor_data_t sensors = animation.sensors;
    animation.overlay_dirty = 0;
    pthread_mutex_unlock(&sensor_lock);
    animation.next_overlay = now + G_USEC_PER_SEC;

    cairo_t *cr = cairo_create(animation.overlay);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    int success = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
    if (success && !display_uses_sensor_slots(&animation.config))
        success = paint_display_overlay(cr, &animation.config, &sensors, &animation.scaling);
    else if (success && sensors.sensor_count)
    {
        Config layout = animation.config;
        /* The stream rotates the composed background and overlay together. */
        layout.display_rotation = 0;
        char *slots[] = {layout.sensor_slot_1, layout.sensor_slot_2, layout.sensor_slot_3};
        for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); i++)
        {
            if (!find_sensor_for_slot(&sensors, slots[i]))
                memcpy(slots[i], "none", sizeof("none"));
        }
        if (display_has_active_sensor_slots(&layout))
            success = paint_display_overlay(cr, &layout, &sensors,
                                             &animation.scaling);
    }
    cairo_destroy(cr);
    return success;
}

static void close_animation(void)
{
    lcd_stream_close(animation.lcd);
    if (animation.sensor_thread_started)
    {
        pthread_mutex_lock(&sensor_lock);
        animation.sensor_stop = 1;
        pthread_cond_signal(&sensor_changed);
        pthread_mutex_unlock(&sensor_lock);
        pthread_join(animation.sensor_thread, NULL);
    }
    if (animation.gif)
        g_object_unref(animation.gif);
    if (animation.cr)
        cairo_destroy(animation.cr);
    if (animation.frame)
        cairo_surface_destroy(animation.frame);
    if (animation.background)
        cairo_surface_destroy(animation.background);
    if (animation.overlay)
        cairo_surface_destroy(animation.overlay);
    memset(&animation, 0, sizeof(animation));
}

void stream_stop(void)
{
    close_animation();
    selected = 0;
}

void stream_start(const Config *config)
{
    if (!is_session_initialized() ||
        (display_uses_sensor_slots(config) && !display_needs_sensor_data(config)) ||
        !config->paths_image_background[0])
        return;

    animation.gif = open_gif(config->paths_image_background);
    if (!animation.gif)
        return;
    const DisplayProfile *profile = cc_released_lcd_profile(config);
    if (!profile)
    {
        close_animation();
        return;
    }

    animation.profile = profile;
    animation.config = *config;
    animation.config.display_width = profile->native_width;
    animation.config.display_height = profile->native_height;
    animation.cr = create_cairo_context(&animation.config, &animation.frame);
    animation.overlay = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, profile->native_width, profile->native_height);
    calculate_scaling_params(&animation.config, &animation.scaling,
                              profile->name);

    if (animation.cr &&
        cairo_surface_status(animation.overlay) == CAIRO_STATUS_SUCCESS)
    {
        animation.lcd = lcd_stream_open(profile);
        const int needs_sensors = display_needs_sensor_data(config);
        if (animation.lcd &&
            (!needs_sensors || pthread_create(&animation.sensor_thread, NULL,
                                              poll_sensors, &animation.config) == 0))
        {
            animation.sensor_thread_started = needs_sensors;
            selected = 1;
            log_message(LOG_STATUS, "LCD stream: %s (%s), up to %u fps",
                        profile->name, display_transport_name(profile->preferred_transport),
                        profile->stream_fps);
            return;
        }
    }
    log_message(LOG_ERROR, "LCD stream could not start");
    close_animation();
}

const DisplayProfile *stream_active_profile(void)
{
    return animation.lcd ? animation.profile : NULL;
}

int stream_selected(void)
{
    return selected;
}

int64_t stream_draw(void)
{
    const DisplayProfile *profile = stream_active_profile();
    if (!profile)
        return 0;

    const gint64 now = g_get_monotonic_time();
    if (now < animation.next_frame)
        return animation.next_frame;
    animation.next_frame = now +
        (G_USEC_PER_SEC + profile->stream_fps - 1) / profile->stream_fps;

    if (update_gif() && update_overlay(now))
    {
        cairo_t *cr = animation.cr;
        const double angle = animation.config.display_rotation * DISPLAY_M_PI / 180.0;
        const double width = animation.config.display_width;
        const double height = animation.config.display_height;
        const double cosine = fabs(cos(angle));
        const double sine = fabs(sin(angle));
        const double scale = fmin(width / (width * cosine + height * sine),
                                  height / (width * sine + height * cosine));
        set_cairo_color(cr, &animation.config.display_background_color);
        cairo_paint(cr);
        cairo_save(cr);
        cairo_translate(cr, width / 2.0, height / 2.0);
        cairo_rotate(cr, angle);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -width / 2.0, -height / 2.0);
        cairo_set_source_surface(cr, animation.background, 0, 0);
        cairo_paint(cr);
        cairo_set_source_surface(cr, animation.overlay, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);

        if (cairo_status(cr) == CAIRO_STATUS_SUCCESS &&
            lcd_stream_send_frame(animation.lcd, animation.frame))
            return animation.next_frame;
    }
    log_message(LOG_ERROR, "LCD stream: frame failed, stopping output");
    close_animation();
    return 0;
}
