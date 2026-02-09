/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Device cache and display detection.
 * @details Caches device UID, detects LCD type (circular vs rectangular).
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "cc_conf.h"
#include "cc_main.h"

/**
 * @brief Secure string copy with bounds checking.
 * @details Performs safe string copying with buffer overflow protection and
 * null termination guarantee.
 */
int cc_safe_strcpy(char *restrict dest, size_t dest_size,
                   const char *restrict src)
{
    if (!dest || !src || dest_size == 0)
    {
        return 0;
    }

    for (size_t i = 0; i < dest_size - 1; i++)
    {
        dest[i] = src[i];
        if (src[i] == '\0')
        {
            return 1;
        }
    }
    dest[dest_size - 1] = '\0';
    return 0;
}

/**
 * @brief Static cache for device information (never changes during runtime).
 * @details Holds the device UID, name, display dimensions, and circular display
 * flag once fetched from the API.
 */
static struct
{
    int initialized;
    char device_uid[128];
    char device_name[CC_NAME_SIZE];
    int screen_width;
    int screen_height;
    int is_circular;
} device_cache = {0};

/**
 * @brief Extract device type from JSON device object.
 * @details Common helper function to extract device type string from JSON
 * device object.
 */
const char *extract_device_type_from_json(const json_t *dev)
{
    if (!dev)
        return NULL;

    const json_t *type_val = json_object_get(dev, "type");
    if (!type_val || !json_is_string(type_val))
        return NULL;

    return json_string_value(type_val);
}

/**
 * @brief Check if a device has a circular display based on device name/type.
 * @details Returns 1 if the device is known to have a circular/round LCD
 * display.
 */
int is_circular_display_device(const char *device_name, int screen_width,
                               int screen_height)
{
    if (!device_name)
        return 0;

    const int is_kraken = (strstr(device_name, "Kraken") != NULL);

    if (is_kraken)
    {
        const int is_large_display = (screen_width > 240 || screen_height > 240);
        return is_large_display ? 1 : 0;
    }

    // Placeholder for future circular display device names
    // Currently no other brands detected, only by display size
    return 0;
}

/**
 * @brief Check if device type is a supported Liquidctl device.
 */
static int is_liquidctl_device(const char *type_str)
{
    if (!type_str)
        return 0;

    const char *liquid_types[] = {"Liquidctl"};
    const size_t num_types = sizeof(liquid_types) / sizeof(liquid_types[0]);

    for (size_t i = 0; i < num_types; i++)
    {
        if (strcmp(type_str, liquid_types[i]) == 0)
            return 1;
    }
    return 0;
}

/**
 * @brief Extract device UID from JSON device object.
 */
static void extract_device_uid(const json_t *dev, char *uid_buffer,
                               size_t buffer_size)
{
    if (!dev || !uid_buffer || buffer_size == 0)
        return;

    const json_t *uid_val = json_object_get(dev, "uid");
    if (uid_val && json_is_string(uid_val))
    {
        const char *uid_str = json_string_value(uid_val);
        snprintf(uid_buffer, buffer_size, "%s", uid_str);
    }
}

/**
 * @brief Extract device name from JSON device object.
 */
static void extract_device_name(const json_t *dev, char *name_buffer,
                                size_t buffer_size)
{
    if (!dev || !name_buffer || buffer_size == 0)
        return;

    const json_t *name_val = json_object_get(dev, "name");
    if (name_val && json_is_string(name_val))
    {
        const char *name_str = json_string_value(name_val);
        snprintf(name_buffer, buffer_size, "%s", name_str);
    }
}

/**
 * @brief Navigate to lcd_info object in device JSON.
 */
static const json_t *get_lcd_info_from_device(const json_t *dev)
{
    if (!dev)
        return NULL;

    const json_t *info = json_object_get(dev, "info");
    if (!info)
        return NULL;

    const json_t *channels = json_object_get(info, "channels");
    if (!channels)
        return NULL;

    const json_t *lcd_channel = json_object_get(channels, "lcd");
    if (!lcd_channel)
        return NULL;

    return json_object_get(lcd_channel, "lcd_info");
}

/**
 * @brief Extract LCD screen dimensions from JSON device object.
 */
static void extract_lcd_dimensions(const json_t *dev, int *width, int *height)
{
    const json_t *lcd_info = get_lcd_info_from_device(dev);
    if (!lcd_info)
        return;

    if (width)
    {
        const json_t *width_val = json_object_get(lcd_info, "screen_width");
        if (width_val && json_is_integer(width_val))
            *width = (int)json_integer_value(width_val);
    }

    if (height)
    {
        const json_t *height_val = json_object_get(lcd_info, "screen_height");
        if (height_val && json_is_integer(height_val))
            *height = (int)json_integer_value(height_val);
    }
}

/**
 * @brief Initialize output parameters to default values.
 */
static void initialize_liquidctl_output_params(
    char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width,
    int *screen_height, char *device_name, size_t name_size)
{
    if (lcd_uid && uid_size > 0)
        lcd_uid[0] = '\0';
    if (found_liquidctl)
        *found_liquidctl = 0;
    if (screen_width)
        *screen_width = 0;
    if (screen_height)
        *screen_height = 0;
    if (device_name && name_size > 0)
        device_name[0] = '\0';
}

/**
 * @brief Extract all information from a Liquidctl device.
 */
static void extract_liquidctl_device_info(const json_t *dev, char *lcd_uid,
                                          size_t uid_size, int *found_liquidctl,
                                          int *screen_width, int *screen_height,
                                          char *device_name, size_t name_size)
{
    if (found_liquidctl)
        *found_liquidctl = 1;

    extract_device_uid(dev, lcd_uid, uid_size);
    extract_device_name(dev, device_name, name_size);
    extract_lcd_dimensions(dev, screen_width, screen_height);
}

/**
 * @brief Search for Liquidctl device in devices array.
 */
static int search_liquidctl_device(const json_t *devices, char *lcd_uid,
                                   size_t uid_size, int *found_liquidctl,
                                   int *screen_width, int *screen_height,
                                   char *device_name, size_t name_size)
{
    const size_t device_count = json_array_size(devices);
    for (size_t i = 0; i < device_count; i++)
    {
        const json_t *dev = json_array_get(devices, i);
        if (!dev)
            continue;

        const char *type_str = extract_device_type_from_json(dev);
        if (!is_liquidctl_device(type_str))
            continue;

        extract_liquidctl_device_info(dev, lcd_uid, uid_size, found_liquidctl,
                                      screen_width, screen_height, device_name,
                                      name_size);
        return 1;
    }

    return 1;
}

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name.
 */
static int parse_liquidctl_data(const char *json, char *lcd_uid,
                                size_t uid_size, int *found_liquidctl,
                                int *screen_width, int *screen_height,
                                char *device_name, size_t name_size)
{
    if (!json)
        return 0;

    initialize_liquidctl_output_params(lcd_uid, uid_size, found_liquidctl,
                                       screen_width, screen_height, device_name,
                                       name_size);

    json_error_t error;
    json_t *root = json_loads(json, 0, &error);
    if (!root)
    {
        log_message(LOG_ERROR, "JSON parse error: %s", error.text);
        return 0;
    }

    const json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices))
    {
        json_decref(root);
        return 0;
    }

    int result = search_liquidctl_device(devices, lcd_uid, uid_size,
                                         found_liquidctl, screen_width,
                                         screen_height, device_name, name_size);
    json_decref(root);
    return result;
}

/**
 * @brief Configure CURL options for device cache request.
 */
static void configure_device_cache_curl(CURL *curl, const char *url,
                                        http_response *chunk,
                                        struct curl_slist **headers)
{
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    *headers = curl_slist_append(NULL, "accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *headers);
}

/**
 * @brief Process device cache API response and populate cache.
 */
static int process_device_cache_response(const http_response *chunk)
{
    int found_liquidctl = 0;
    int result = parse_liquidctl_data(
        chunk->data, device_cache.device_uid, sizeof(device_cache.device_uid),
        &found_liquidctl, &device_cache.screen_width, &device_cache.screen_height,
        device_cache.device_name, sizeof(device_cache.device_name));

    if (result && found_liquidctl)
    {
        device_cache.initialized = 1;
        device_cache.is_circular = is_circular_display_device(
            device_cache.device_name, device_cache.screen_width,
            device_cache.screen_height);

        const char *shape_mode = device_cache.is_circular
                                     ? "scaled (circular)"
                                     : "unscaled (rectangular)";
        log_message(LOG_STATUS, "Device cache initialized: %s (%dx%d pixel, %s)",
                    device_cache.device_name, device_cache.screen_width,
                    device_cache.screen_height, shape_mode);
        return 1;
    }

    return 0;
}

/**
 * @brief Initialize device cache by fetching device information once.
 */
static int initialize_device_cache(const Config *config)
{
    if (device_cache.initialized)
        return 1;

    if (!config)
        return 0;

    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    char url[512];
    int ret = snprintf(url, sizeof(url), "%s/devices", config->daemon_address);
    if (ret >= (int)sizeof(url) || ret < 0)
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    http_response chunk = {0};
    chunk.data = malloc(4096);
    if (!chunk.data)
    {
        curl_easy_cleanup(curl);
        return 0;
    }
    chunk.size = 0;
    chunk.capacity = 4096;

    struct curl_slist *headers = NULL;
    configure_device_cache_curl(curl, url, &chunk, &headers);

    int success = 0;
    if (curl_easy_perform(curl) == CURLE_OK)
    {
        success = process_device_cache_response(&chunk);
    }

    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

/**
 * @brief Get complete Liquidctl device information from cache.
 */
int get_liquidctl_data(const Config *config, char *device_uid, size_t uid_size,
                       char *device_name, size_t name_size, int *screen_width,
                       int *screen_height)
{
    if (!initialize_device_cache(config))
    {
        return 0;
    }

    if (device_uid && uid_size > 0)
    {
        cc_safe_strcpy(device_uid, uid_size, device_cache.device_uid);
    }
    if (device_name && name_size > 0)
    {
        cc_safe_strcpy(device_name, name_size, device_cache.device_name);
    }
    if (screen_width)
        *screen_width = device_cache.screen_width;
    if (screen_height)
        *screen_height = device_cache.screen_height;

    return 1;
}

/**
 * @brief Initialize device cache - public interface.
 */
int init_device_cache(const Config *config)
{
    return initialize_device_cache(config);
}

/**
 * @brief Validate device cache dimensions.
 */
static int validate_device_dimensions(void)
{
    if (device_cache.screen_width <= 0 || device_cache.screen_height <= 0)
    {
        log_message(
            LOG_WARNING,
            "Device has invalid screen dimensions (%dx%d), using config values",
            device_cache.screen_width, device_cache.screen_height);
        return 0;
    }
    return 1;
}

/**
 * @brief Update config display dimension if not set.
 */
static int update_dimension(uint16_t *config_dim, int device_dim,
                            const char *dim_name)
{
    const uint16_t original_value = *config_dim;

    if (*config_dim == 0)
    {
        *config_dim = (uint16_t)device_dim;
        log_message(LOG_INFO,
                    "Display %s set from device: %d (config.json not set)",
                    dim_name, *config_dim);
        return 1;
    }

    if (original_value != (uint16_t)device_dim)
    {
        log_message(LOG_INFO, "Display %s from config.json: %d (device reports %d)",
                    dim_name, *config_dim, device_dim);
    }
    else
    {
        log_message(LOG_INFO, "Display %s: %d (device and default match)", dim_name,
                    *config_dim);
    }
    return 0;
}

/**
 * @brief Update config with device screen dimensions.
 */
int update_config_from_device(Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config pointer for device update");
        return 0;
    }

    if (!device_cache.initialized)
    {
        log_message(
            LOG_WARNING,
            "Device cache not initialized, using config values as fallback");
        return 0;
    }

    if (!validate_device_dimensions())
        return 0;

    int updated = 0;
    updated |= update_dimension(&config->display_width, device_cache.screen_width,
                                "width");
    updated |= update_dimension(&config->display_height,
                                device_cache.screen_height, "height");

    return updated;
}
