/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 */

#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

typedef enum
{
    DISPLAY_SHAPE_UNKNOWN = 0,
    DISPLAY_SHAPE_RECTANGULAR,
    DISPLAY_SHAPE_CIRCULAR,
    DISPLAY_SHAPE_ROUNDED_SQUARE
} DisplayShape;

typedef enum
{
    DISPLAY_TRANSPORT_IMAGE_UPLOAD = 0,
    DISPLAY_TRANSPORT_STREAM_RGB565 = 0x06,
    DISPLAY_TRANSPORT_STREAM_Q565 = 0x08
} DisplayTransport;

/**
 * @brief Immutable hardware display metadata.
 * @details Direct LCD streaming requires a supported device and firmware.
 */
typedef struct
{
    const char *name;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t native_width;
    uint16_t native_height;
    DisplayShape shape;
    double visible_diameter_ratio;
    double center_x_ratio;
    double center_y_ratio;
    DisplayTransport preferred_transport;
    uint8_t stream_firmware_major; /**< 0 if there is no firmware restriction */
    uint8_t stream_fps; /**< Maximum frame rate for the direct LCD stream */
    const char *match_token_1;
    const char *match_token_2;
    const char *match_token_3;
} DisplayProfile;

/**
 * @brief Resolve a known display profile.
 * @details VID:PID takes precedence. Model-token matching is the fallback for
 * CoolerControl versions that do not expose USB IDs via /devices.
 */
const DisplayProfile *resolve_display_profile(uint16_t vendor_id,
                                              uint16_t product_id,
                                              const char *device_name,
                                              int screen_width,
                                              int screen_height);

/** Match the model name when CoolerControl's LCD channel is disabled. */
const DisplayProfile *identify_display_profile(const char *device_name);

/** @brief Calculate the narrowest circle chord spanning a vertical region. */
int calculate_circle_chord_bounds(double center_x, double center_y,
                                  double radius, double region_y,
                                  double region_height, double width_factor,
                                  double *safe_x, double *safe_width);

const char *display_shape_name(DisplayShape shape);
const char *display_transport_name(DisplayTransport transport);

#endif // PROFILE_H
