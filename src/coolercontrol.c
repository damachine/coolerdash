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
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"

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
 * @brief Secure memory allocation with initialization.
 * @details Allocates memory using calloc to ensure zero-initialization and prevent uninitialized data access.
 */
void *cc_secure_malloc(size_t size)
{
    if (size == 0 || size > CC_MAX_SAFE_ALLOC_SIZE)
    {
        return NULL;
    }

    return calloc(1, size);
}

/**
 * @brief Initialize HTTP response buffer with specified capacity.
 * @details Allocates memory for HTTP response data with proper initialization.
 */
int cc_init_response_buffer(struct http_response *response, size_t initial_capacity)
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
 * @brief Validate HTTP response buffer integrity.
 * @details Checks if response buffer is in valid state for operations.
 */
int cc_validate_response_buffer(const struct http_response *response)
{
    return (response &&
            response->data &&
            response->size <= response->capacity);
}

/**
 * @brief Cleanup HTTP response buffer and free memory.
 * @details Properly frees allocated memory and resets buffer state.
 */
void cc_cleanup_response_buffer(struct http_response *response)
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
const char *extract_device_type_from_json(json_t *dev)
{
    if (!dev)
        return NULL;

    // Get device type
    json_t *type_val = json_object_get(dev, "type");
    if (!type_val || !json_is_string(type_val))
        return NULL;

    // Extract device type string
    return json_string_value(type_val);
}

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is called by libcurl to write the response data into a dynamically allocated buffer with automatic reallocation when needed.
 */
size_t write_callback(char *contents, size_t size, size_t nmemb, void *userp)
{
    if (!userp || !contents)
        return 0;
    struct http_response *response = (struct http_response *)userp;

    // Validate input
    const size_t realsize = size * nmemb;
    const size_t new_size = response->size + realsize + 1;

    // Reallocate if needed
    if (new_size > response->capacity)
    {
        /* Grow capacity: choose larger of new_size or 1.5x current */
        const size_t new_capacity = (new_size > response->capacity * 3 / 2) ? new_size : response->capacity * 3 / 2;

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
    }

    // Copy data with bounds safety (capacity already ensured)
    if (realsize > 0)
    {
        memmove(response->data + response->size, contents, realsize);
        response->size += realsize;
        response->data[response->size] = '\0';
    }

    return realsize;
}

/**
 * @brief Helper to get a JSON string field value.
 * @details Returns the string value if present and valid, NULL otherwise.
 */
static const char *get_json_string_field(json_t *obj, const char *key)
{
    if (!obj || !key)
        return NULL;
    json_t *val = json_object_get(obj, key);
    if (!val || !json_is_string(val))
        return NULL;
    return json_string_value(val);
}

/**
 * @brief Helper to safely copy a string into a bounded buffer.
 * @details Ensures null termination and handles NULL source strings.
 */
static void safe_copy_to_buf(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
        return;
    if (src)
        snprintf(dst, dst_size, "%s", src);
    else
        dst[0] = '\0';
}

/**
 * @brief Helper to extract LCD dimensions from a device JSON object.
 * @details Extracts screen_width and screen_height fields if available.
 */
static void extract_lcd_dimensions(json_t *dev, int *out_width, int *out_height)
{
    if (out_width)
        *out_width = 0;
    if (out_height)
        *out_height = 0;
    if (!dev)
        return;

    json_t *info = json_object_get(dev, "info");
    json_t *channels = info ? json_object_get(info, "channels") : NULL;
    json_t *lcd_channel = channels ? json_object_get(channels, "lcd") : NULL;
    json_t *lcd_info = lcd_channel ? json_object_get(lcd_channel, "lcd_info") : NULL;
    if (!lcd_info)
        return;

    if (out_width)
    {
        json_t *w = json_object_get(lcd_info, "screen_width");
        if (w && json_is_integer(w))
            *out_width = (int)json_integer_value(w);
    }
    if (out_height)
    {
        json_t *h = json_object_get(lcd_info, "screen_height");
        if (h && json_is_integer(h))
            *out_height = (int)json_integer_value(h);
    }
}

/**
 * @brief Helper that processes a single device JSON object and extracts
 * @details Liquidctl-specific fields; returns 1 if the device is Liquidctl
 */
static int process_device(json_t *dev, char *lcd_uid, size_t uid_size,
                          int *found_liquidctl, int *screen_width, int *screen_height,
                          char *device_name, size_t name_size)
{
    if (!dev)
        return 0;

    const char *type_str = extract_device_type_from_json(dev);
    if (!type_str)
        return 0;

    /* Only accept exact "Liquidctl" device type */
    if (strcmp(type_str, "Liquidctl") != 0)
        return 0;

    if (found_liquidctl)
        *found_liquidctl = 1;

    /* UID and name */
    const char *uid_val = get_json_string_field(dev, "uid");
    safe_copy_to_buf(lcd_uid, uid_size, uid_val);

    const char *name_val = get_json_string_field(dev, "name");
    safe_copy_to_buf(device_name, name_size, name_val);

    /* Dimensions */
    if (screen_width || screen_height)
    {
        int w = 0, h = 0;
        extract_lcd_dimensions(dev, &w, &h);
        if (screen_width)
            *screen_width = w;
        if (screen_height)
            *screen_height = h;
    }

    return 1;
}

static int parse_liquidctl_data(const char *json, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size)
{
    if (!json)
        return 0;

    if (lcd_uid && uid_size > 0)
        lcd_uid[0] = '\0';
    if (device_name && name_size > 0)
        device_name[0] = '\0';
    if (found_liquidctl)
        *found_liquidctl = 0;
    if (screen_width)
        *screen_width = 0;
    if (screen_height)
        *screen_height = 0;

    json_error_t error;
    json_t *root = json_loads(json, 0, &error);
    if (!root)
    {
        log_message(LOG_ERROR, "JSON parse error: %s", error.text);
        return 0;
    }

    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices))
    {
        json_decref(root);
        return 0;
    }

    const size_t device_count = json_array_size(devices);
    for (size_t i = 0; i < device_count; i++)
    {
        json_t *dev = json_array_get(devices, i);
        if (!dev)
            continue;

        if (process_device(dev, lcd_uid, uid_size, found_liquidctl, screen_width, screen_height, device_name, name_size))
        {
            json_decref(root);
            return 1;
        }
    }

    json_decref(root);
    return 1;
}

/**
 * @brief Helper: build devices URL.
 * @details Writes the devices endpoint into the provided buffer, returns 1 on success.
 */
static int build_devices_url(const Config *config, char *url, size_t url_size)
{
    if (!config || !url)
        return 0;
    int ret = snprintf(url, url_size, "%s/devices", config->daemon_address);
    if (ret < 0 || (size_t)ret >= url_size)
        return 0;
    return 1;
}

/**
 * @brief Helper: initialize response chunk.
 * @details Allocates and initializes a http_response buffer.
 */
static int init_response_chunk(struct http_response *chunk, size_t initial_capacity)
{
    if (!chunk || initial_capacity == 0 || initial_capacity > CC_MAX_SAFE_ALLOC_SIZE)
        return 0;
    chunk->data = malloc(initial_capacity);
    if (!chunk->data)
    {
        chunk->size = 0;
        chunk->capacity = 0;
        return 0;
    }
    chunk->size = 0;
    chunk->capacity = initial_capacity;
    chunk->data[0] = '\0';
    return 1;
}

/**
 * @brief Fetch devices JSON from daemon and parse Liquidctl device info.
 * @details Helper that encapsulates CURL setup, request execution and JSON parsing.
 */
static int fetch_and_parse_devices(const Config *config,
                                   char *out_uid, size_t uid_size,
                                   int *out_found_liquidctl,
                                   int *out_screen_width, int *out_screen_height,
                                   char *out_device_name, size_t name_size)
{
    if (!config)
        return 0;

    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    char url[512];
    if (!build_devices_url(config, url, sizeof(url)))
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    struct http_response chunk = {0};
    const size_t initial_capacity = 4096;
    if (!init_response_chunk(&chunk, initial_capacity))
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    int parse_result = 0;
    if (curl_easy_perform(curl) == CURLE_OK)
    {
        parse_result = parse_liquidctl_data(chunk.data,
                                            out_uid, uid_size,
                                            out_found_liquidctl,
                                            out_screen_width, out_screen_height,
                                            out_device_name, name_size);
    }

    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return parse_result;
}

/**
 * @brief Initialize device cache by fetching device information once.
 * @details Populates the static cache with device UID, name, and display dimensions.
 */
static int initialize_device_cache(const Config *config)
{
    if (device_cache.initialized)
    {
        return 1; // Already initialized
    }

    if (!config)
        return 0;

    int found_liquidctl = 0;
    int result = fetch_and_parse_devices(config,
                                         device_cache.device_uid, sizeof(device_cache.device_uid),
                                         &found_liquidctl,
                                         &device_cache.screen_width, &device_cache.screen_height,
                                         device_cache.device_name, sizeof(device_cache.device_name));
    if (result && found_liquidctl)
    {
        device_cache.initialized = 1;
        log_message(LOG_STATUS, "Device cache initialized: %s (%dx%d pixel)",
                    device_cache.device_name, device_cache.screen_width, device_cache.screen_height);
    }

    return device_cache.initialized;
}

/**
 * @brief Build login URL and user credential string into provided buffers.
 * @details Ensures buffers are large enough and returns 1 on success, 0 on failure.
 */
static int build_login_url_and_userpwd(const Config *config, char *login_url, size_t login_url_size, char *userpwd, size_t userpwd_size)
{
    if (!config || !login_url || !userpwd)
        return 0;

    int written_url = snprintf(login_url, login_url_size, "%s/login", config->daemon_address);
    if (written_url < 0 || (size_t)written_url >= login_url_size)
    {
        return 0;
    }

    int written_pwd = snprintf(userpwd, userpwd_size, "CCAdmin:%s", config->daemon_password);
    if (written_pwd < 0 || (size_t)written_pwd >= userpwd_size)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Configure CURL options for the login request and return allocated headers list.
 * @details Caller is responsible for freeing the returned curl_slist* with curl_slist_free_all().
 */
static struct curl_slist *prepare_login_headers_and_options(const Config *config, const char *login_url, const char *userpwd)
{
    struct curl_slist *headers = NULL;

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, login_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);

    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);

    if (strncmp(config->daemon_address, "https://", 8) == 0)
    {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    return headers;
}

/**
 * @brief Execute the login request and perform cleanup of temporary data.
 * @details Returns 1 on success (HTTP 200 or 204 and CURLE_OK), 0 otherwise.
 */
static int execute_login_and_cleanup(struct curl_slist *headers, char *userpwd, size_t userpwd_size)
{
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    if (userpwd && userpwd_size > 0)
    {
        memset(userpwd, 0, userpwd_size);
    }
    if (headers)
    {
        curl_slist_free_all(headers);
    }

    return (res == CURLE_OK && (response_code == 200 || response_code == 204));
}

static int perform_login_request(const Config *config)
{
    char login_url[CC_URL_SIZE];
    char userpwd[CC_USERPWD_SIZE];

    if (!build_login_url_and_userpwd(config, login_url, sizeof(login_url), userpwd, sizeof(userpwd)))
    {
        return 0;
    }

    struct curl_slist *headers = prepare_login_headers_and_options(config, login_url, userpwd);
    return execute_login_and_cleanup(headers, userpwd, sizeof(userpwd));
}

int init_coolercontrol_session(const Config *config)
{
    /* Initialize cURL and create handle */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle)
    {
        return 0;
    }

    /* Create unique cookie jar path using PID */
    int written_cookie = snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar),
                                  "/tmp/coolerdash_cookie_%d.txt", getpid());
    if (written_cookie < 0 || (size_t)written_cookie >= sizeof(cc_session.cookie_jar))
    {
        cc_session.cookie_jar[sizeof(cc_session.cookie_jar) - 1] = '\0';
    }

    /* Configure cookie handling */
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR, cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE, cc_session.cookie_jar);

    /* Perform login via helper to keep this function small */
    if (perform_login_request(config))
    {
        cc_session.session_initialized = 1;
        return 1;
    }

    log_message(LOG_ERROR, "Login failed");
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
 * @brief Create a multipart MIME form for LCD image upload.
 * @details Adds required fields: mode, brightness, orientation and the image file.
 */
static int add_mime_field(curl_mime *form, const char *name, const char *data)
{
    curl_mimepart *p = curl_mime_addpart(form);
    if (!p)
        return 0;
    if (curl_mime_name(p, name) != CURLE_OK)
        return 0;
    if (curl_mime_data(p, data, CURL_ZERO_TERMINATED) != CURLE_OK)
        return 0;
    return 1;
}

static int add_mime_file(curl_mime *form, const char *fieldname, const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) != 0)
        return 0;
    curl_mimepart *p = curl_mime_addpart(form);
    if (!p)
        return 0;
    if (curl_mime_name(p, fieldname) != CURLE_OK)
        return 0;
    if (curl_mime_filedata(p, filepath) != CURLE_OK)
        return 0;
    if (curl_mime_type(p, "image/png") != CURLE_OK)
        return 0;
    return 1;
}

/**
 * @brief Populate the multipart form with standard fields (mode, brightness, orientation).
 * @details Adds the required non-file form fields to the given curl_mime form.
 */
static int populate_upload_fields(curl_mime *form, const Config *config)
{
    char tmp[16];

    if (!form || !config)
    {
        return 0;
    }

    if (!add_mime_field(form, "mode", "image"))
    {
        return 0;
    }

    if (snprintf(tmp, sizeof(tmp), "%d", config->lcd_brightness) < 0)
    {
        return 0;
    }
    if (!add_mime_field(form, "brightness", tmp))
    {
        return 0;
    }

    if (snprintf(tmp, sizeof(tmp), "%d", config->lcd_orientation) < 0)
    {
        return 0;
    }
    if (!add_mime_field(form, "orientation", tmp))
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Append the image file to the multipart form and log helpful message on failure.
 * @details Tries to add the image file part; if it fails but the file exists, a specific error is logged.
 */
static int append_image_file_to_form(curl_mime *form, const char *image_path)
{
    if (!form || !image_path)
    {
        return 0;
    }

    if (add_mime_file(form, "images[]", image_path))
    {
        return 1;
    }

    struct stat st;
    if (stat(image_path, &st) == 0)
    {
        log_message(LOG_ERROR, "Failed to set image file data");
    }

    return 0;
}

/**
 * @brief Create a multipart MIME form for LCD image upload.
 * @details Adds required fields: mode, brightness, orientation and the image file.
 */
static curl_mime *create_upload_mime(const Config *config, const char *image_path)
{
    if (!cc_session.curl_handle || !config || !image_path)
    {
        return NULL;
    }

    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    if (!form)
    {
        log_message(LOG_ERROR, "Failed to initialize multipart form");
        return NULL;
    }

    if (!populate_upload_fields(form, config) || !append_image_file_to_form(form, image_path))
    {
        curl_mime_free(form);
        return NULL;
    }

    return form;
}

/**
 * @brief Build upload URL for LCD image endpoint.
 * @details Helper to construct and validate the upload URL into the provided buffer.
 */
static int build_upload_url(char *buf, size_t bufsize, const Config *config, const char *device_uid)
{
    if (!buf || !config || !device_uid)
    {
        return 0;
    }
    int sret = snprintf(buf, bufsize, "%s/devices/%s/settings/lcd/lcd/images",
                        config->daemon_address, device_uid);
    if (sret < 0 || (size_t)sret >= bufsize)
    {
        return 0;
    }
    return 1;
}

/**
 * @brief Configure common CURL options, response buffer and headers for upload.
 * @details Centralizes response buffer initialization, SSL options, write callback and header list setup.
 */
static int setup_curl_upload_options(const Config *config, struct http_response *out_response,
                                     struct curl_slist **out_headers)
{
    if (!cc_session.curl_handle || !config || !out_response || !out_headers)
    {
        log_message(LOG_ERROR, "Invalid parameters to setup_curl_upload_options");
        return 0;
    }

    if (!cc_init_response_buffer(out_response, 4096))
    {
        log_message(LOG_ERROR, "Failed to initialize response buffer");
        return 0;
    }

    if (strncmp(config->daemon_address, "https://", 8) == 0)
    {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, out_response);

    *out_headers = NULL;
    *out_headers = curl_slist_append(*out_headers, "User-Agent: CoolerDash/1.0");
    *out_headers = curl_slist_append(*out_headers, "Accept: application/json");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, *out_headers);

    return 1;
}

/**
 * @brief Validate parameters for prepare_lcd_upload.
 * @details Small helper to centralize and document parameter validation so the main
 */
static int validate_prepare_lcd_upload_params(const Config *config, const char *image_path, const char *device_uid,
                                              curl_mime **out_form, struct http_response *out_response,
                                              struct curl_slist **out_headers)
{
    if (!cc_session.curl_handle || !config || !image_path || !device_uid || !out_form || !out_response || !out_headers)
    {
        log_message(LOG_ERROR, "Invalid parameters to prepare_lcd_upload");
        return 0;
    }
    return 1;
}

/**
 * @brief Prepare CURL options, MIME form and response buffer for LCD image upload.
 * @details Helper that centralizes URL construction, form creation and CURL option setup to keep the public function small.
 */
static int prepare_lcd_upload(const Config *config, const char *image_path, const char *device_uid,
                              curl_mime **out_form, struct http_response *out_response,
                              struct curl_slist **out_headers)
{
    /* Validate parameters via small helper to reduce this function's branching */
    if (!validate_prepare_lcd_upload_params(config, image_path, device_uid, out_form, out_response, out_headers))
    {
        return 0;
    }

    char upload_url[CC_URL_SIZE];
    if (!build_upload_url(upload_url, sizeof(upload_url), config, device_uid))
    {
        log_message(LOG_ERROR, "Upload URL too long or invalid");
        return 0;
    }

    /* Create MIME form (may perform several internal checks) */
    *out_form = create_upload_mime(config, image_path);
    if (!*out_form)
    {
        log_message(LOG_ERROR, "Failed to create upload form");
        return 0;
    }

    /* Configure CURL options, initialize response buffer and headers */
    if (!setup_curl_upload_options(config, out_response, out_headers))
    {
        log_message(LOG_ERROR, "Failed to setup CURL options for upload");
        curl_mime_free(*out_form);
        *out_form = NULL;
        return 0;
    }

    /* Set URL, form and HTTP method after the common options are prepared */
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, *out_form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");

    return 1;
}

/**
 * @brief Perform upload using CURL and process response.
 * @details Executes the CURL request, checks HTTP code, logs server response on failure and resets CURL options.
 */
static int perform_lcd_upload_and_finalize(curl_mime *form, struct http_response *response, struct curl_slist *headers)
{
    /* suppress unused parameter warnings - parameters are kept for symmetry and potential future use */
    (void)form;
    (void)headers;
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);

    int success = (res == CURLE_OK && http_response_code == 200);
    if (!success)
    {
        log_message(LOG_ERROR, "LCD upload failed: CURL code %d, HTTP code %ld", res, http_response_code);
        if (response && response->data && response->size > 0)
        {
            response->data[response->size] = '\0';
            log_message(LOG_ERROR, "Server response: %s", response->data);
        }
    }

    /* Reset curl options modified for upload */
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);

    return success;
}

/**
 * @brief Internal helper that performs the prepare->perform->cleanup sequence for LCD image upload.
 * @details Extracted to reduce cyclomatic complexity of the public wrapper function.
 */
static int perform_send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid)
{
    curl_mime *form = NULL;
    struct http_response response = {0};
    struct curl_slist *headers = NULL;

    if (!prepare_lcd_upload(config, image_path, device_uid, &form, &response, &headers))
    {
        cc_cleanup_response_buffer(&response);
        if (headers)
            curl_slist_free_all(headers);
        return 0;
    }

    int success = perform_lcd_upload_and_finalize(form, &response, headers);

    /* Cleanup resources */
    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    if (form)
        curl_mime_free(form);

    return success;
}

/**
 * @brief Sends an image to the LCD display.
 * @details This function uploads a PNG image to the LCD display of the Liquidctl device using the CoolerControl API. It prepares the multipart form, sets up CURL options, performs the upload, and handles cleanup.
 */
int send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid)
{
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized)
    {
        log_message(LOG_ERROR, "Invalid parameters or session not initialized");
        return 0;
    }

    return perform_send_image_to_lcd(config, image_path, device_uid);
}
