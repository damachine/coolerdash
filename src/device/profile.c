/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 */

#include "profile.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>

#define NZXT_VENDOR_ID 0x1e71

/*
 * Geometry is deliberately conservative: only verified native resolutions and
 * shapes live here. A zero FPS value means that no stable limit is known yet.
 */
static const DisplayProfile display_profiles[] = {
    {"NZXT Kraken Z series", NZXT_VENDOR_ID, 0x3008, 320, 320,
     DISPLAY_SHAPE_CIRCULAR, 1.0, 0.5, 0.5,
     DISPLAY_TRANSPORT_IMAGE_UPLOAD, 0x00, 0,
     "kraken z", NULL, NULL},
    {"NZXT Kraken 2023 Elite", NZXT_VENDOR_ID, 0x300c, 640, 640,
     DISPLAY_SHAPE_CIRCULAR, 1.0, 0.5, 0.5,
     DISPLAY_TRANSPORT_IMAGE_UPLOAD, 0x00, 0,
     "kraken", "2023", "elite"},
    {"NZXT Kraken 2023", NZXT_VENDOR_ID, 0x300e, 240, 240,
     DISPLAY_SHAPE_RECTANGULAR, 1.0, 0.5, 0.5,
     DISPLAY_TRANSPORT_IMAGE_UPLOAD, 0x00, 0,
     "kraken", "2023", NULL},
    {"NZXT Kraken Elite (2024)", NZXT_VENDOR_ID, 0x3012, 640, 640,
     DISPLAY_SHAPE_CIRCULAR, 1.0, 0.5, 0.5,
     DISPLAY_TRANSPORT_IMAGE_UPLOAD, 0x00, 0,
     "kraken", "2024", "elite"},
    {"NZXT Kraken Plus (2025)", NZXT_VENDOR_ID, 0x3014, 240, 240,
     DISPLAY_SHAPE_RECTANGULAR, 1.0, 0.5, 0.5,
     DISPLAY_TRANSPORT_IMAGE_UPLOAD, 0x00, 0,
     "kraken", "plus", NULL},
};

static int contains_token_ci(const char *text, const char *token)
{
    if (!token || token[0] == '\0')
        return 1;
    if (!text)
        return 0;

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        size_t j = 0;
        while (token[j] != '\0' && text[i + j] != '\0' &&
               tolower((unsigned char)text[i + j]) ==
                   tolower((unsigned char)token[j]))
        {
            j++;
        }
        if (token[j] == '\0')
            return 1;
    }

    return 0;
}

static int dimensions_match(const DisplayProfile *profile,
                            int screen_width, int screen_height)
{
    return screen_width == profile->native_width &&
           screen_height == profile->native_height;
}

const DisplayProfile *resolve_display_profile(uint16_t vendor_id,
                                              uint16_t product_id,
                                              const char *device_name,
                                              int screen_width,
                                              int screen_height)
{
    const size_t count = sizeof(display_profiles) / sizeof(display_profiles[0]);

    if (vendor_id != 0 && product_id != 0)
    {
        for (size_t i = 0; i < count; i++)
        {
            if (display_profiles[i].vendor_id == vendor_id &&
                display_profiles[i].product_id == product_id)
                return &display_profiles[i];
        }
    }

    for (size_t i = 0; i < count; i++)
    {
        const DisplayProfile *profile = &display_profiles[i];
        if (dimensions_match(profile, screen_width, screen_height) &&
            contains_token_ci(device_name, profile->match_token_1) &&
            contains_token_ci(device_name, profile->match_token_2) &&
            contains_token_ci(device_name, profile->match_token_3))
        {
            return profile;
        }
    }

    return NULL;
}

int calculate_circle_chord_bounds(double center_x, double center_y,
                                  double radius, double region_y,
                                  double region_height, double width_factor,
                                  double *safe_x, double *safe_width)
{
    if (!safe_x || !safe_width || radius <= 0.0)
        return 0;

    const double height = fmax(0.0, region_height);
    const double factor = fmax(0.0, fmin(1.0, width_factor));
    const double top_distance = fabs(region_y - center_y);
    const double bottom_distance = fabs(region_y + height - center_y);
    const double limiting_distance = fmax(top_distance, bottom_distance);

    if (limiting_distance >= radius)
        return 0;

    const double half_chord =
        sqrt((radius * radius) - (limiting_distance * limiting_distance));
    const double width = 2.0 * half_chord * factor;
    if (width <= 0.0)
        return 0;

    *safe_width = width;
    *safe_x = center_x - (width / 2.0);
    return 1;
}

const char *display_shape_name(DisplayShape shape)
{
    switch (shape)
    {
    case DISPLAY_SHAPE_RECTANGULAR:
        return "rectangular";
    case DISPLAY_SHAPE_CIRCULAR:
        return "circular";
    case DISPLAY_SHAPE_ROUNDED_SQUARE:
        return "rounded-square";
    default:
        return "unknown";
    }
}

const char *display_transport_name(DisplayTransport transport)
{
    return transport == DISPLAY_TRANSPORT_STREAM_BGR888
               ? "stream-bgr888"
               : "image-upload";
}
