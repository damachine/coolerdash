/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 *    This software is provided "as is", without warranty of any kind, express or implied.
 *    I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief CoolerControl API interface for LCD device communication and sensor data.
 * @details Provides functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "config.h"
#include "coolercontrol.h"

/**
 * @brief Secure string copy with bounds checking.
 * @details Performs safe string copying with buffer overflow protection and null termination guarantee.
 */
int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src)
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
 * @brief Initialize HTTP response buffer with specified capacity.
 * @details Allocates memory for HTTP response data with proper initialization.
 */
int cc_init_response_buffer(http_response *response, size_t initial_capacity)
{
    if (!response || initial_capacity == 0 || initial_capacity > CC_MAX_SAFE_ALLOC_SIZE)
    {
        return 0;
    }

    response->data = malloc(initial_capacity);
    if (!response->data)
    {
        response->size = 0;
        response->capacity = 0;
        return 0;
    }

    response->size = 0;
    response->capacity = initial_capacity;
    response->data[0] = '\0';
    return 1;
}

/**
 * @brief Cleanup HTTP response buffer and free memory.
 * @details Properly frees allocated memory and resets buffer state.
 */
void cc_cleanup_response_buffer(http_response *response)
{
    if (!response)
    {
        return;
    }

    if (response->data)
    {
        free(response->data);
        response->data = NULL;
    }
    response->size = 0;
    response->capacity = 0;
}

/**
 * @brief Structure to hold CoolerControl session state.
 * @details Contains the CURL handle, cookie jar path, and session initialization status.
 */
typedef struct
{
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;

/**
 * @brief Global CoolerControl session state.
 * @details Holds the state of the CoolerControl session, including the CURL handle and session initialization status.
 */
static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0};

/**
 * @brief Static cache for device information (never changes during runtime).
 * @details Holds the device UID, name, and display dimensions once fetched from the API. Used to avoid redundant API calls and improve performance.
 */
static struct
{
    int initialized;
    char device_uid[CC_UID_SIZE];
    char device_name[CC_NAME_SIZE];
    int screen_width;
    int screen_height;
} device_cache = {0}; // Strings always written via snprintf/cc_safe_strcpy (bounded)

/**
 * @brief Initialize device cache by fetching device information once.
 * @details Populates the static cache with device UID, name, and display dimensions.
 */
static int initialize_device_cache(const Config *config);

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name from Liquidctl devices.
 * @details This function parses the JSON response from the CoolerControl API to find Liquidctl devices, extracting their UID, display dimensions, and device name.
 */
static int parse_liquidctl_data(const char *json, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size);

/**
 * @brief Extract device type from JSON device object.
 * @details Common helper function to extract device type string from JSON device object. Returns NULL if extraction fails.
 */
const char *extract_device_type_from_json(const json_t *dev)
{
    if (!dev)
        return NULL;

    // Get device type
    const json_t *type_val = json_object_get(dev, "type");
    if (!type_val || !json_is_string(type_val))
        return NULL;

    // Extract device type string
    return json_string_value(type_val);
}

/**
 * @brief Check if a device has a circular display based on device name/type.
 * @details Returns 1 if the device is known to have a circular/round LCD display.
 *          Uses an internal database of known circular display devices.
 *
 *          Display shape rules for NZXT Kraken devices:
 *          - NZXT Kraken 2023 with 240x240 = RECTANGULAR (no inscribe factor)
 *          - NZXT Kraken with >240x240 (e.g., 320x320) = CIRCULAR (with inscribe factor)
 *
 *          Known devices with CIRCULAR displays:
 *          - NZXT Kraken Z (320x320 circular)
 *          - NZXT Kraken Elite with >240x240 (circular)
 *
 *          Known devices with RECTANGULAR displays:
 *          - NZXT Kraken 2023 (240x240 rectangular LCD)
 *          - NZXT Kraken X (rectangular LCD)
 *          - Corsair iCUE H100i/H115i/H150i Elite LCD (480x480 square)
 *          - Corsair Commander Pro LCD (rectangular)
 */
int is_circular_display_device(const char *device_name, int screen_width, int screen_height)
{
    if (!device_name)
        return 0;

    // Check if device is NZXT Kraken series
    const int is_kraken = (strstr(device_name, "Kraken") != NULL);

    if (is_kraken)
    {
        // NZXT Kraken display shape depends on resolution:
        // - 240x240 or smaller = RECTANGULAR
        // - Larger than 240x240 = CIRCULAR
        const int is_large_display = (screen_width > 240 || screen_height > 240);
        return is_large_display ? 1 : 0;
    }

    // Database of non-Kraken devices with CIRCULAR displays
    // Add new circular display devices here (non-Kraken only)
    const char *circular_devices[] = {
        // Add other brands with circular displays here
        // Example: "Corsair LCD Circular Model"
    };

    const size_t num_circular = sizeof(circular_devices) / sizeof(circular_devices[0]);

    // Check if device name contains any known circular display identifier
    for (size_t i = 0; i < num_circular; i++)
    {
        if (strstr(device_name, circular_devices[i]) != NULL)
        {
            return 1; // Circular display detected
        }
    }

    // Default: assume rectangular display if not in circular database
    // This is safer as most displays are rectangular
    return 0;
}

/**
 * @brief Check if device type is a supported Liquidctl device.
 * @details Compares device type string against list of supported types.
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
 * @details Extracts and copies the UID string to the provided buffer.
 */
static void extract_device_uid(const json_t *dev, char *uid_buffer, size_t buffer_size)
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
 * @details Extracts and copies the device name to the provided buffer.
 */
static void extract_device_name(const json_t *dev, char *name_buffer, size_t buffer_size)
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
 * @details Traverses the nested JSON structure to find lcd_info.
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
 * @details Extracts screen width and height from nested LCD info structure.
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
 * @brief Reallocate response buffer if needed.
 * @details Grows buffer capacity using exponential growth strategy.
 */
static int reallocate_response_buffer(http_response *response, size_t required_size)
{
    const size_t new_capacity = (required_size > response->capacity * 3 / 2)
                                    ? required_size
                                    : response->capacity * 3 / 2;

    char *ptr = realloc(response->data, new_capacity);
    if (!ptr)
    {
        log_message(LOG_ERROR, "Memory allocation failed for response data: %zu bytes", new_capacity);
        free(response->data);
        response->data = NULL;
        response->size = 0;
        response->capacity = 0;
        return 0;
    }

    response->data = ptr;
    response->capacity = new_capacity;
    return 1;
}

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is called by libcurl to write the response data into a dynamically allocated buffer with automatic reallocation when needed.
 */
size_t write_callback(const void *contents, size_t size, size_t nmemb, http_response *response)
{
    const size_t realsize = size * nmemb;
    const size_t new_size = response->size + realsize + 1;

    if (new_size > response->capacity)
    {
        if (!reallocate_response_buffer(response, new_size))
            return 0;
    }

    if (realsize > 0)
    {
        memmove(response->data + response->size, contents, realsize);
        response->size += realsize;
        response->data[response->size] = '\0';
    }

    return realsize;
}

/**
 * @brief Initialize output parameters to default values.
 * @details Sets all output parameters to their default/empty state.
 */
static void initialize_liquidctl_output_params(char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size)
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
 * @details Populates output parameters with device UID, name, and display dimensions.
 */
static void extract_liquidctl_device_info(const json_t *dev, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size)
{
    if (found_liquidctl)
        *found_liquidctl = 1;

    extract_device_uid(dev, lcd_uid, uid_size);
    extract_device_name(dev, device_name, name_size);
    extract_lcd_dimensions(dev, screen_width, screen_height);
}

/**
 * @brief Search for Liquidctl device in devices array.
 * @details Iterates through devices array and extracts info when Liquidctl device is found.
 */
static int search_liquidctl_device(const json_t *devices, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size)
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

        extract_liquidctl_device_info(dev, lcd_uid, uid_size, found_liquidctl, screen_width, screen_height, device_name, name_size);
        return 1;
    }

    return 1;
}

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name from Liquidctl devices.
 * @details This function parses the JSON response from the CoolerControl API to find Liquidctl devices, extracting their UID, display dimensions, and device name.
 */
static int parse_liquidctl_data(const char *json, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size)
{
    if (!json)
        return 0;

    initialize_liquidctl_output_params(lcd_uid, uid_size, found_liquidctl, screen_width, screen_height, device_name, name_size);

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

    int result = search_liquidctl_device(devices, lcd_uid, uid_size, found_liquidctl, screen_width, screen_height, device_name, name_size);
    json_decref(root);
    return result;
}

/**
 * @brief Configure CURL options for device cache request.
 * @details Sets up URL, headers, timeout and write callback for the API request.
 */
static void configure_device_cache_curl(CURL *curl, const char *url, http_response *chunk, struct curl_slist **headers)
{
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    // Set HTTP headers
    *headers = curl_slist_append(NULL, "accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *headers);
}

/**
 * @brief Process device cache API response and populate cache.
 * @details Parses the JSON response and updates the device cache if Liquidctl device found.
 */
static int process_device_cache_response(const http_response *chunk)
{
    int found_liquidctl = 0;
    int result = parse_liquidctl_data(chunk->data,
                                      device_cache.device_uid, sizeof(device_cache.device_uid),
                                      &found_liquidctl,
                                      &device_cache.screen_width, &device_cache.screen_height,
                                      device_cache.device_name, sizeof(device_cache.device_name));

    // Initialize cache if Liquidctl device found
    if (result && found_liquidctl)
    {
        device_cache.initialized = 1;
        log_message(LOG_STATUS, "Device cache initialized: %s (%dx%d pixel)",
                    device_cache.device_name, device_cache.screen_width, device_cache.screen_height);
        return 1;
    }

    return 0;
}

/**
 * @brief Initialize device cache by fetching device information once.
 * @details Populates the static cache with device UID, name, and display dimensions.
 */
static int initialize_device_cache(const Config *config)
{
    // Check if already initialized
    if (device_cache.initialized)
        return 1;

    // Validate input
    if (!config)
        return 0;

    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    // Construct URL for devices endpoint
    char url[512];
    int ret = snprintf(url, sizeof(url), "%s/devices", config->daemon_address);
    if (ret >= (int)sizeof(url) || ret < 0)
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    // Initialize response buffer
    http_response chunk = {0};
    chunk.data = malloc(4096);
    if (!chunk.data)
    {
        curl_easy_cleanup(curl);
        return 0;
    }
    chunk.size = 0;
    chunk.capacity = 4096;

    // Configure CURL and perform request
    struct curl_slist *headers = NULL;
    configure_device_cache_curl(curl, url, &chunk, &headers);

    // Perform request and process response
    int success = 0;
    if (curl_easy_perform(curl) == CURLE_OK)
    {
        success = process_device_cache_response(&chunk);
    }

    // Clean up resources
    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

/**
 * @brief Validate snprintf result for buffer overflow.
 * @details Checks if snprintf truncated output.
 */
static int validate_snprintf(int written, size_t buffer_size, char *buffer)
{
    if (written < 0 || (size_t)written >= buffer_size)
    {
        buffer[buffer_size - 1] = '\0';
        return 0;
    }
    return 1;
}

/**
 * @brief Build login URL and credentials for CoolerControl session.
 * @details Constructs the login URL and username:password string from config.
 */
static int build_login_credentials(const Config *config, char *login_url, size_t url_size, char *userpwd, size_t pwd_size)
{
    int written_url = snprintf(login_url, url_size, "%s/login", config->daemon_address);
    if (!validate_snprintf(written_url, url_size, login_url))
        return 0;

    int written_pwd = snprintf(userpwd, pwd_size, "CCAdmin:%s", config->daemon_password);
    if (!validate_snprintf(written_pwd, pwd_size, userpwd))
        return 0;

    return 1;
}

/**
 * @brief Configure CURL for CoolerControl login request.
 * @details Sets up all CURL options including authentication, headers and SSL.
 */
static struct curl_slist *configure_login_curl(CURL *curl, const Config *config, const char *login_url, const char *userpwd)
{
    curl_easy_setopt(curl, CURLOPT_URL, login_url);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

    // Set HTTP headers for login request
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Enable SSL verification for HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    return headers;
}

/**
 * @brief Check login response status.
 * @details Validates CURL result and HTTP response code.
 */
static int is_login_successful(CURLcode res, long response_code)
{
    return (res == CURLE_OK && (response_code == 200 || response_code == 204));
}

/**
 * @brief Initializes the CoolerControl session (CURL setup and login).
 * @details This function initializes the CURL library, sets up the session cookie jar, constructs the login URL and credentials, and performs a login to the CoolerControl API. No handshake or session authentication is performed beyond basic login.
 */
int init_coolercontrol_session(const Config *config)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle)
        return 0;

    int written_cookie = snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar),
                                  "/tmp/coolerdash_cookie_%d.txt", getpid());
    if (written_cookie < 0 || (size_t)written_cookie >= sizeof(cc_session.cookie_jar))
    {
        cc_session.cookie_jar[sizeof(cc_session.cookie_jar) - 1] = '\0';
    }

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR, cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE, cc_session.cookie_jar);

    char login_url[CC_URL_SIZE];
    char userpwd[CC_USERPWD_SIZE];
    if (!build_login_credentials(config, login_url, sizeof(login_url), userpwd, sizeof(userpwd)))
        return 0;

    struct curl_slist *headers = configure_login_curl(cc_session.curl_handle, config, login_url, userpwd);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    memset(userpwd, 0, sizeof(userpwd));

    if (headers)
        curl_slist_free_all(headers);

    if (is_login_successful(res, response_code))
    {
        cc_session.session_initialized = 1;
        return 1;
    }

    log_message(LOG_ERROR, "Login failed: CURL code %d, HTTP code %ld", res, response_code);
    return 0;
}

/**
 * @brief Checks if the CoolerControl session is initialized.
 * @details This function returns whether the session has been successfully initialized and is ready for use.
 */
int is_session_initialized(void)
{
    return cc_session.session_initialized;
}

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details This function performs cleanup of the CURL handle, removes the cookie jar file, and marks the session as uninitialized. It ensures that all resources are properly released and no memory leaks occur.
 */
void cleanup_coolercontrol_session(void)
{
    static int cleanup_done = 0;
    if (cleanup_done)
        return;

    int all_cleaned = 1;

    // Clean up CURL handle
    if (cc_session.curl_handle)
    {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }

    // Perform global CURL cleanup
    curl_global_cleanup();

    // Remove cookie jar file
    if (unlink(cc_session.cookie_jar) != 0)
    {
        all_cleaned = 0;
    }

    // Mark session as uninitialized
    cc_session.session_initialized = 0;

    // Set cleanup flag only if all operations succeeded
    if (all_cleaned)
    {
        cleanup_done = 1;
    }
}

/**
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from CoolerControl API.
 * @details This function retrieves the UID, name, and screen dimensions of the Liquidctl device by calling the main device info function.
 */
int get_liquidctl_data(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height)
{
    // Initialize cache if not already done
    if (!initialize_device_cache(config))
    {
        return 0;
    }

    // Copy values from cache to output buffers with safe string handling
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
 * @details Public function to initialize the device cache for external callers.
 */
int init_device_cache(const Config *config)
{
    return initialize_device_cache(config);
}

/**
 * @brief Validate device cache dimensions.
 * @details Checks if device has valid screen dimensions.
 */
static int validate_device_dimensions(void)
{
    if (device_cache.screen_width <= 0 || device_cache.screen_height <= 0)
    {
        log_message(LOG_WARNING, "Device has invalid screen dimensions (%dx%d), using config values",
                    device_cache.screen_width, device_cache.screen_height);
        return 0;
    }
    return 1;
}

/**
 * @brief Update config display dimension if not set.
 * @details Sets dimension from device if config value is 0 (commented out).
 */
static int update_dimension(uint16_t *config_dim, int device_dim, const char *dim_name)
{
    if (*config_dim == 0)
    {
        *config_dim = (uint16_t)device_dim;
        log_message(LOG_INFO, "Display %s set from device: %d (config.ini was commented out)",
                    dim_name, *config_dim);
        return 1;
    }

    log_message(LOG_INFO, "Display %s from config.ini: %d (overrides device value %d)",
                dim_name, *config_dim, device_dim);
    return 0;
}

/**
 * @brief Update config with device screen dimensions (device values override config).
 * @details Reads screen dimensions from device cache and updates config if device values are available. Device values take priority over config.ini values.
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
        log_message(LOG_WARNING, "Device cache not initialized, using config values as fallback");
        return 0;
    }

    if (!validate_device_dimensions())
        return 0;

    int updated = 0;
    updated |= update_dimension(&config->display_width, device_cache.screen_width, "width");
    updated |= update_dimension(&config->display_height, device_cache.screen_height, "height");

    return updated;
}

/**
 * @brief Add a string field to multipart form with error checking.
 * @details Helper function to add a named string field to curl_mime form.
 */
static int add_mime_field(curl_mime *form, const char *field_name, const char *field_value)
{
    curl_mimepart *field = curl_mime_addpart(form);
    if (!field)
    {
        log_message(LOG_ERROR, "Failed to create %s field", field_name);
        return 0;
    }

    CURLcode result = curl_mime_name(field, field_name);
    if (result != CURLE_OK)
    {
        log_message(LOG_ERROR, "Failed to set %s field name: %s", field_name, curl_easy_strerror(result));
        return 0;
    }

    result = curl_mime_data(field, field_value, CURL_ZERO_TERMINATED);
    if (result != CURLE_OK)
    {
        log_message(LOG_ERROR, "Failed to set %s field data: %s", field_name, curl_easy_strerror(result));
        return 0;
    }

    return 1;
}

/**
 * @brief Add image file to multipart form with error checking.
 * @details Adds the image file field with proper MIME type and validation.
 */
static int add_image_file_field(curl_mime *form, const char *image_path)
{
    curl_mimepart *field = curl_mime_addpart(form);
    if (!field)
    {
        log_message(LOG_ERROR, "Failed to create image field");
        return 0;
    }

    CURLcode result = curl_mime_name(field, "images[]");
    if (result != CURLE_OK)
    {
        log_message(LOG_ERROR, "Failed to set image field name: %s", curl_easy_strerror(result));
        return 0;
    }

    result = curl_mime_filedata(field, image_path);
    if (result != CURLE_OK)
    {
        struct stat file_stat;
        if (stat(image_path, &file_stat) == 0)
        {
            log_message(LOG_ERROR, "Failed to set image file data: %s", curl_easy_strerror(result));
        }
        return 0;
    }

    result = curl_mime_type(field, "image/png");
    if (result != CURLE_OK)
    {
        log_message(LOG_ERROR, "Failed to set image MIME type: %s", curl_easy_strerror(result));
        return 0;
    }

    return 1;
}

/**
 * @brief Build multipart form for LCD image upload.
 * @details Constructs the complete multipart form with mode, brightness, orientation and image fields.
 */
static curl_mime *build_lcd_upload_form(const Config *config, const char *image_path)
{
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    if (!form)
    {
        log_message(LOG_ERROR, "Failed to initialize multipart form");
        return NULL;
    }

    // Add mode field
    if (!add_mime_field(form, "mode", "image"))
    {
        curl_mime_free(form);
        return NULL;
    }

    // Add brightness field
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    if (!add_mime_field(form, "brightness", brightness_str))
    {
        curl_mime_free(form);
        return NULL;
    }

    // Add orientation field
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    if (!add_mime_field(form, "orientation", orientation_str))
    {
        curl_mime_free(form);
        return NULL;
    }

    // Add image file
    if (!add_image_file_field(form, image_path))
    {
        curl_mime_free(form);
        return NULL;
    }

    return form;
}

/**
 * @brief Configure CURL for LCD image upload request.
 * @details Sets up URL, form data, SSL and response handling.
 */
static struct curl_slist *configure_lcd_upload_curl(const Config *config, const char *upload_url, curl_mime *form, http_response *response)
{
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");

    // Enable SSL verification for HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0)
    {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    // Set write callback
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, response);

    // Add headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);

    return headers;
}

/**
 * @brief Cleanup CURL options after LCD upload.
 * @details Resets all CURL options set during the upload request.
 */
static void cleanup_lcd_upload_curl(void)
{
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);
}

/**
 * @brief Validate upload request parameters.
 * @details Checks all required parameters for LCD upload.
 */
static int validate_upload_params(const char *image_path, const char *device_uid)
{
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized)
    {
        log_message(LOG_ERROR, "Invalid parameters or session not initialized");
        return 0;
    }
    return 1;
}

/**
 * @brief Check LCD upload response.
 * @details Validates CURL result and HTTP response code for LCD upload.
 */
static int check_upload_response(CURLcode res, long http_response_code, const http_response *response)
{
    if (res == CURLE_OK && http_response_code == 200)
        return 1;

    log_message(LOG_ERROR, "LCD upload failed: CURL code %d, HTTP code %ld", res, http_response_code);
    if (response->data && response->size > 0)
    {
        response->data[response->size] = '\0';
        log_message(LOG_ERROR, "Server response: %s", response->data);
    }
    return 0;
}

/**
 * @brief Sends an image to the LCD display.
 * @details This function uploads an image to the LCD display using a multipart HTTP PUT request. It requires the session to be initialized and the device UID to be available.
 */
int send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid)
{
    if (!validate_upload_params(image_path, device_uid))
        return 0;

    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url), "%s/devices/%s/settings/lcd/lcd/images?log=false",
             config->daemon_address, device_uid);

    curl_mime *form = build_lcd_upload_form(config, image_path);
    if (!form)
        return 0;

    http_response response = {0};
    if (!cc_init_response_buffer(&response, 4096))
    {
        log_message(LOG_ERROR, "Failed to initialize response buffer");
        curl_mime_free(form);
        return 0;
    }

    struct curl_slist *headers = configure_lcd_upload_curl(config, upload_url, form, &response);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);

    int success = check_upload_response(res, http_response_code, &response);

    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    curl_mime_free(form);
    cleanup_lcd_upload_curl();

    return success;
}