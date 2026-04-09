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
#include <ctype.h>
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
 * @brief Cache for all device names and types (populated from /devices).
 * @details Maps device UID to display name and type string.
 */
static struct
{
    char uid[128];
    char name[CC_NAME_SIZE];
    char type[16];
} device_name_cache[MAX_DEVICE_NAME_CACHE];
static int device_name_cache_count = 0;

/**
 * @brief Resets device cache for config reload (SIGHUP).
 * @details Clears all cached device data so it will be re-fetched from the API.
 */
void reset_device_cache(void)
{
    memset(&device_cache, 0, sizeof(device_cache));
    memset(device_name_cache, 0, sizeof(device_name_cache));
    device_name_cache_count = 0;
}

/**
 * @brief Get device display name by UID.
 */
const char *get_device_name_by_uid(const char *device_uid)
{
    if (!device_uid)
        return "";
    for (int i = 0; i < device_name_cache_count; i++)
    {
        if (strcmp(device_name_cache[i].uid, device_uid) == 0)
            return device_name_cache[i].name;
    }
    return "";
}

/**
 * @brief Get device type string by UID.
 */
const char *get_device_type_by_uid(const char *device_uid)
{
    if (!device_uid)
        return NULL;
    for (int i = 0; i < device_name_cache_count; i++)
    {
        if (strcmp(device_name_cache[i].uid, device_uid) == 0)
            return device_name_cache[i].type;
    }
    return NULL;
}

/**
 * @brief Extract device type from JSON device object.
 * @details Common helper function to extract device type string from JSON
 * device object. Checks "type" first (/devices), falls back to "d_type" (/status).
 */
const char *extract_device_type_from_json(const json_t *dev)
{
    if (!dev)
        return NULL;

    const json_t *type_val = json_object_get(dev, "type");
    if (!type_val || !json_is_string(type_val))
    {
        /* Fallback: /status endpoint uses "d_type" instead of "type" */
        type_val = json_object_get(dev, "d_type");
        if (!type_val || !json_is_string(type_val))
            return NULL;
    }

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

    // Unknown device: detect by display size (>240px = circular LCD)
    return (screen_width > 240 || screen_height > 240) ? 1 : 0;
}

/**
 * @brief Check if a device exposes a usable UID for LCD uploads.
 */
static int has_usable_device_uid(const json_t *dev)
{
    if (!dev)
        return 0;

    const json_t *uid_val = json_object_get(dev, "uid");
    return uid_val && json_is_string(uid_val) &&
           json_string_value(uid_val) && json_string_value(uid_val)[0] != '\0';
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

    const char *channel_keys[] = {"lcd", "display", "screen"};
    const size_t key_count = sizeof(channel_keys) / sizeof(channel_keys[0]);

    for (size_t i = 0; i < key_count; i++)
    {
        const json_t *lcd_channel = json_object_get(channels, channel_keys[i]);
        if (!lcd_channel || !json_is_object(lcd_channel))
            continue;

        const json_t *lcd_info = json_object_get(lcd_channel, "lcd_info");
        if (lcd_info && json_is_object(lcd_info))
            return lcd_info;
    }

    return NULL;
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
 * @brief Initialize cached LCD output parameters to default values.
 */
static void initialize_cached_lcd_output_params(
    char *lcd_uid, size_t uid_size, int *found_lcd_device, int *screen_width,
    int *screen_height, char *device_name, size_t name_size)
{
    if (lcd_uid && uid_size > 0)
        lcd_uid[0] = '\0';
    if (found_lcd_device)
        *found_lcd_device = 0;
    if (screen_width)
        *screen_width = 0;
    if (screen_height)
        *screen_height = 0;
    if (device_name && name_size > 0)
        device_name[0] = '\0';
}

/**
 * @brief Extract all information from a CoolerControl LCD device.
 */
static void extract_lcd_device_info(const json_t *dev, char *lcd_uid,
                                    size_t uid_size, int *found_lcd_device,
                                    int *screen_width, int *screen_height,
                                    char *device_name, size_t name_size)
{
    if (found_lcd_device)
        *found_lcd_device = 1;

    extract_device_uid(dev, lcd_uid, uid_size);
    extract_device_name(dev, device_name, name_size);
    extract_lcd_dimensions(dev, screen_width, screen_height);
}

/**
 * @brief Check if a CoolerControl device has an LCD display.
 * @details Verifies that a supported channel exposes lcd_info with valid
 * screen dimensions.
 */
static int has_lcd_display(const json_t *dev)
{
    const json_t *lcd_info = get_lcd_info_from_device(dev);
    if (!lcd_info)
        return 0;

    const json_t *w = json_object_get(lcd_info, "screen_width");
    const json_t *h = json_object_get(lcd_info, "screen_height");
    if (!w || !h || !json_is_integer(w) || !json_is_integer(h))
        return 0;

    return (json_integer_value(w) > 0 && json_integer_value(h) > 0);
}

/**
 * @brief Convert ASCII character to lower-case without locale dependency.
 */
static char ascii_tolower(char ch)
{
    return (char)tolower((unsigned char)ch);
}

/**
 * @brief Check if a string contains a token case-insensitively.
 */
static int contains_token_ci(const char *haystack, const char *needle)
{
    if (!haystack || !needle || needle[0] == '\0')
        return 0;

    const size_t needle_len = strlen(needle);
    for (size_t i = 0; haystack[i] != '\0'; i++)
    {
        size_t j = 0;
        while (j < needle_len && haystack[i + j] != '\0' &&
               ascii_tolower(haystack[i + j]) == ascii_tolower(needle[j]))
        {
            j++;
        }
        if (j == needle_len)
            return 1;
    }

    return 0;
}

/**
 * @brief Match a pattern list against name, type, and UID.
 * @details Patterns are separated by commas, semicolons, or newlines.
 */
static int pattern_list_matches(const char *pattern_list, const char *device_name,
                                const char *type_str, const char *device_uid)
{
    if (!pattern_list || pattern_list[0] == '\0')
        return 0;

    char token[64];
    size_t token_len = 0;

    for (size_t i = 0;; i++)
    {
        const char ch = pattern_list[i];
        const int is_separator =
            (ch == '\0' || ch == '\n' || ch == '\r' || ch == ',' ||
             ch == ';');

        if (!is_separator)
        {
            if (!(token_len == 0 && isspace((unsigned char)ch)) &&
                token_len + 1 < sizeof(token))
            {
                token[token_len++] = ch;
            }
        }

        if (is_separator)
        {
            while (token_len > 0 &&
                   isspace((unsigned char)token[token_len - 1]))
            {
                token_len--;
            }

            token[token_len] = '\0';
            if (token_len > 0 &&
                (contains_token_ci(device_name, token) ||
                 contains_token_ci(type_str, token) ||
                 contains_token_ci(device_uid, token)))
            {
                return 1;
            }

            token_len = 0;

            if (ch == '\0')
                break;
        }
    }

    return 0;
}

/**
 * @brief Detect common motherboard/mainboard false positives.
 */
static int looks_like_mainboard_device(const char *device_name)
{
    static const char *tokens[] = {
        "mainboard", "motherboard", "chipset", "vrm",
        "z690", "z790", "b650", "b660",
        "x670", "x870", "x570", "b550"};

    if (!device_name)
        return 0;

    for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
    {
        if (contains_token_ci(device_name, tokens[i]))
            return 1;
    }

    return 0;
}

/**
 * @brief Get effective LCD detection mode.
 */
static const char *get_detection_mode(const Config *config)
{
    if (!config || config->device_detection_mode[0] == '\0')
        return "strict";
    return config->device_detection_mode;
}

/**
 * @brief Score an LCD candidate based on type, name, and dimensions.
 */
static int score_lcd_candidate(const Config *config, const char *device_uid,
                               const char *device_name, const char *type_str,
                               int screen_width, int screen_height)
{
    int score = 0;
    const char *mode = get_detection_mode(config);
    const int allowlisted = pattern_list_matches(
        config ? config->device_detection_allowlist : NULL, device_name,
        type_str, device_uid);
    const int blocklisted = pattern_list_matches(
        config ? config->device_detection_blocklist : NULL, device_name,
        type_str, device_uid);

    if (allowlisted)
        return 1000 + (screen_width * screen_height);

    if (blocklisted)
        return -1000;

    if (type_str && strcmp(type_str, "Liquidctl") == 0)
        score += 30;
    else if (type_str &&
             (strcmp(type_str, "Hwmon") == 0 ||
              strcmp(type_str, "CustomSensors") == 0 ||
              strcmp(type_str, "CPU") == 0 || strcmp(type_str, "GPU") == 0))
        score -= 40;
    else if (type_str && type_str[0] != '\0')
        score += 8;

    if (contains_token_ci(device_name, "kraken") ||
        contains_token_ci(device_name, "elite") ||
        contains_token_ci(device_name, "hydro") ||
        contains_token_ci(device_name, "capellix") ||
        contains_token_ci(device_name, "lcd") ||
        contains_token_ci(device_name, "display") ||
        contains_token_ci(device_name, "screen") ||
        contains_token_ci(device_name, "aio"))
    {
        score += 24;
    }

    if (looks_like_mainboard_device(device_name))
        score -= 80;

    if (screen_width == screen_height)
        score += 6;
    else
        score += 3;

    if (screen_width >= 240 && screen_height >= 240)
        score += 4;

    if (strcmp(mode, "relaxed") == 0)
        return (score < 0) ? 0 : score;

    if (strcmp(mode, "balanced") == 0)
        return (score >= 0) ? score : -1;

    if (looks_like_mainboard_device(device_name))
        return -1;

    return (score >= 20) ? score : -1;
}

/**
 * @brief Search for the best CoolerControl LCD device candidate.
 * @details Uses UID, LCD metadata, and configurable allow/block heuristics.
 */
static int search_lcd_device(const Config *config, const json_t *devices, char *lcd_uid,
                             size_t uid_size, int *found_lcd_device,
                             int *screen_width, int *screen_height,
                             char *device_name, size_t name_size)
{
    const json_t *best_dev = NULL;
    int best_score = -1001;
    int candidate_count = 0;

    const size_t device_count = json_array_size(devices);
    for (size_t i = 0; i < device_count; i++)
    {
        const json_t *dev = json_array_get(devices, i);
        if (!dev)
            continue;

        const char *type_str = extract_device_type_from_json(dev);
        const json_t *name_val = json_object_get(dev, "name");
        const char *name = name_val ? json_string_value(name_val) : "unknown";
        char uid_value[128] = {0};
        int width = 0;
        int height = 0;

        if (!has_usable_device_uid(dev))
        {
            log_message(LOG_INFO,
                        "Skipping CoolerControl device without usable UID: %s [%s]",
                        name ? name : "unknown",
                        type_str ? type_str : "unknown type");
            continue;
        }

        if (!has_lcd_display(dev))
        {
            log_message(LOG_INFO,
                        "Skipping CoolerControl device without LCD info: %s [%s]",
                        name ? name : "unknown",
                        type_str ? type_str : "unknown type");
            continue;
        }

        extract_device_uid(dev, uid_value, sizeof(uid_value));
        extract_lcd_dimensions(dev, &width, &height);

        int score = score_lcd_candidate(config, uid_value, name, type_str, width,
                                        height);
        if (score < 0)
        {
            log_message(LOG_INFO,
                        "Skipping filtered LCD candidate: %s [%s] (%dx%d)",
                        name ? name : "unknown",
                        type_str ? type_str : "unknown type", width, height);
            continue;
        }

        candidate_count++;
        if (score > best_score)
        {
            best_score = score;
            best_dev = dev;
        }

        log_message(LOG_INFO,
                    "LCD candidate score=%d: %s [%s] (%dx%d, uid=%s)", score,
                    name ? name : "unknown",
                    type_str ? type_str : "unknown type", width, height,
                    uid_value[0] != '\0' ? uid_value : "n/a");
    }

    if (best_dev)
    {
        extract_lcd_device_info(best_dev, lcd_uid, uid_size, found_lcd_device,
                                screen_width, screen_height, device_name,
                                name_size);
        return 1;
    }

    if (candidate_count == 0)
    {
        log_message(LOG_WARNING,
                    "No suitable LCD device candidates found after filtering");
    }

    return 0;
}

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name.
 */
static int parse_lcd_device_data(const Config *config, const char *json,
                                 char *lcd_uid, size_t uid_size,
                                 int *found_lcd_device, int *screen_width,
                                 int *screen_height, char *device_name,
                                 size_t name_size)
{
    if (!json)
        return 0;

    initialize_cached_lcd_output_params(lcd_uid, uid_size, found_lcd_device,
                                        screen_width, screen_height,
                                        device_name, name_size);

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

    int result = search_lcd_device(config, devices, lcd_uid, uid_size,
                                   found_lcd_device, screen_width,
                                   screen_height, device_name, name_size);
    json_decref(root);
    return result;
}

/**
 * @brief Configure CURL options for device cache request.
 */
static void configure_device_cache_curl(CURL *curl, const Config *config,
                                        const char *url,
                                        http_response *chunk,
                                        struct curl_slist **headers)
{
    (void)config;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    *headers = curl_slist_append(NULL, "accept: application/json");

    const char *bearer = get_session_access_token();
    if (bearer && bearer[0] != '\0')
        *headers = curl_slist_append(*headers, bearer);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *headers);
}

/**
 * @brief Populate device name cache from parsed JSON devices array.
 * @details Caches UID, name, and type for all devices (not just Liquidctl).
 */
static void populate_device_name_cache(const char *json_data)
{
    if (!json_data)
        return;

    json_error_t error;
    json_t *root = json_loads(json_data, 0, &error);
    if (!root)
        return;

    const json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices))
    {
        json_decref(root);
        return;
    }

    device_name_cache_count = 0;
    const size_t count = json_array_size(devices);
    for (size_t i = 0; i < count && device_name_cache_count < MAX_DEVICE_NAME_CACHE; i++)
    {
        const json_t *dev = json_array_get(devices, i);
        if (!dev)
            continue;

        const json_t *uid_val = json_object_get(dev, "uid");
        const json_t *name_val = json_object_get(dev, "name");
        const char *type_str = extract_device_type_from_json(dev);

        if (!uid_val || !json_is_string(uid_val))
            continue;

        int idx = device_name_cache_count;
        cc_safe_strcpy(device_name_cache[idx].uid,
                       sizeof(device_name_cache[idx].uid),
                       json_string_value(uid_val));

        if (name_val && json_is_string(name_val))
            cc_safe_strcpy(device_name_cache[idx].name,
                           sizeof(device_name_cache[idx].name),
                           json_string_value(name_val));
        else
            device_name_cache[idx].name[0] = '\0';

        if (type_str)
            cc_safe_strcpy(device_name_cache[idx].type,
                           sizeof(device_name_cache[idx].type),
                           type_str);
        else
            device_name_cache[idx].type[0] = '\0';

        device_name_cache_count++;
    }

    log_message(LOG_INFO, "Device name cache: %d devices cached",
                device_name_cache_count);
    json_decref(root);
}

/**
 * @brief Process device cache API response and populate cache.
 */
static int process_device_cache_response(const Config *config,
                                         const http_response *chunk)
{
    /* Populate device name cache for ALL devices (used by sensor system) */
    populate_device_name_cache(chunk->data);

    int found_lcd_device = 0;
    int result = parse_lcd_device_data(
        config, chunk->data, device_cache.device_uid,
        sizeof(device_cache.device_uid),
        &found_lcd_device, &device_cache.screen_width,
        &device_cache.screen_height, device_cache.device_name,
        sizeof(device_cache.device_name));

    if (result && found_lcd_device)
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
    configure_device_cache_curl(curl, config, url, &chunk, &headers);

    int success = 0;
    if (curl_easy_perform(curl) == CURLE_OK)
    {
        success = process_device_cache_response(config, &chunk);
    }

    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

/**
 * @brief Get cached LCD device information.
 */
int get_cached_lcd_device_data(const Config *config, char *device_uid,
                               size_t uid_size, char *device_name,
                               size_t name_size, int *screen_width,
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

    if (device_dim <= 0)
    {
        log_message(LOG_WARNING,
                    "Display %s from device is invalid: %d, keeping config value %u",
                    dim_name, device_dim, original_value);
        return 0;
    }

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
        *config_dim = (uint16_t)device_dim;
        log_message(LOG_INFO,
                    "Display %s updated from device: %d (config.json had %u)",
                    dim_name, *config_dim, original_value);
        return 1;
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
