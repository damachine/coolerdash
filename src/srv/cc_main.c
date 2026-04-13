/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Session management, authentication, LCD image upload.
 * @details HTTP client for CoolerControl REST API.
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
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "cc_conf.h"
#include "cc_main.h"

/**
 * @brief Initialize HTTP response buffer with specified capacity.
 * @details Allocates memory for HTTP response data with proper initialization.
 */
int cc_init_response_buffer(http_response *response, size_t initial_capacity)
{
    if (!response || initial_capacity == 0 ||
        initial_capacity > CC_MAX_SAFE_ALLOC_SIZE)
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
 * @details Contains the CURL handle, cookie jar path, and session
 * initialization status.
 */
typedef struct
{
    CURL *curl_handle;
    char access_token[CC_BEARER_HEADER_SIZE];
    int session_initialized;
} CoolerControlSession;

/**
 * @brief Global CoolerControl session state.
 * @details Holds the state of the CoolerControl session, including the CURL
 * handle and session initialization status.
 */
static CoolerControlSession cc_session = {
    .curl_handle = NULL, .access_token = {0}, .session_initialized = 0};

/**
 * @brief Reallocate response buffer if needed.
 * @details Grows buffer capacity using exponential growth strategy.
 */
static int reallocate_response_buffer(http_response *response,
                                      size_t required_size)
{
    const size_t new_capacity = (required_size > response->capacity * 3 / 2)
                                    ? required_size
                                    : response->capacity * 3 / 2;

    char *ptr = realloc(response->data, new_capacity);
    if (!ptr)
    {
        log_message(LOG_ERROR,
                    "Memory allocation failed for response data: %zu bytes",
                    new_capacity);
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
 * @details This function is called by libcurl to write the response data into a
 * dynamically allocated buffer with automatic reallocation when needed.
 */
size_t write_callback(const void *contents, size_t size, size_t nmemb,
                      http_response *response)
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
 * @brief Initializes the CoolerControl session (CURL setup and login).
 * @details This function initializes the CURL library and stores the Bearer
 * authorization header used for all CoolerControl API requests.
 */
int init_coolercontrol_session(const Config *config)
{
    if (!config || config->access_token[0] == '\0')
    {
        log_message(LOG_ERROR,
                    "CoolerControl access token missing; token-only authentication is required");
        return 0;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle)
        return 0;

    int written = snprintf(cc_session.access_token, sizeof(cc_session.access_token),
                           "Authorization: Bearer %s", config->access_token);
    if (!validate_snprintf(written, sizeof(cc_session.access_token),
                           cc_session.access_token))
    {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
        curl_global_cleanup();
        log_message(LOG_ERROR, "Access token header exceeds maximum size");
        return 0;
    }

    cc_session.session_initialized = 1;
    log_message(LOG_STATUS, "Session initialized using Bearer token");
    return 1;
}

/**
 * @brief Checks if the CoolerControl session is initialized.
 * @details This function returns whether the session has been successfully
 * initialized and is ready for use.
 */
int is_session_initialized(void) { return cc_session.session_initialized; }

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details This function performs cleanup of the CURL handle and marks the
 * session as uninitialized.
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

    // Mark session as uninitialized
    cc_session.session_initialized = 0;
    cc_session.access_token[0] = '\0';

    // Set cleanup flag only if all operations succeeded
    if (all_cleaned)
    {
        cleanup_done = 1;
    }
}

/**
 * @brief Resets the CoolerControl session for config reload (SIGHUP).
 * @details Unlike cleanup_coolercontrol_session(), this does NOT call
 * curl_global_cleanup() and does NOT set the one-shot cleanup_done guard.
 * The session can be re-initialized via init_coolercontrol_session().
 */
void reset_coolercontrol_session(void)
{
    if (cc_session.curl_handle)
    {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }

    cc_session.session_initialized = 0;
    cc_session.access_token[0] = '\0';
}

/**
 * @brief Returns the active Bearer auth header string (or empty string).
 */
const char *get_session_access_token(void)
{
    return cc_session.access_token;
}

/**
 * @brief Reset CURL options after a request to prepare for the next one.
 * @details Clears all request-specific CURL options (URL, body, headers, etc.).
 */
static void reset_curl_request_options(void)
{
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);
}

/**
 * @brief Validate upload request parameters.
 * @details Checks all required parameters for LCD upload.
 */
static int validate_upload_params(const char *image_path,
                                  const char *device_uid)
{
    if (!cc_session.curl_handle || !image_path || !device_uid ||
        !cc_session.session_initialized)
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
static int check_upload_response(CURLcode res, long http_response_code,
                                 const http_response *response)
{
    if (res == CURLE_OK && http_response_code == 200)
        return 1;

    log_message(LOG_ERROR, "LCD upload failed: CURL code %d, HTTP code %ld", res,
                http_response_code);
    if (response->data && response->size > 0)
    {
        response->data[response->size] = '\0';
        log_message(LOG_ERROR, "Server response: %s", response->data);
    }
    return 0;
}

/**
 * @brief Sends an image path to the LCD display via JSON settings endpoint.
 * @details Instead of uploading the full image binary via multipart, this sends
 * the file path to CoolerControl which reads the image directly from disk.
 * Uses PUT /devices/{uid}/settings/lcd/lcd with LcdSettings JSON body.
 */
int send_image_to_lcd(const Config *config, const char *image_path,
                      const char *device_uid)
{
    if (!validate_upload_params(image_path, device_uid))
        return 0;

    char upload_url[CC_URL_SIZE];
    int written = snprintf(upload_url, sizeof(upload_url),
                           "%s/devices/%s/settings/lcd/lcd",
                           config->daemon_address, device_uid);
    if (!validate_snprintf(written, sizeof(upload_url), upload_url))
    {
        log_message(LOG_ERROR, "LCD settings URL truncated");
        return 0;
    }

    // Build JSON body: LcdSettings with image_file_processed path
    json_t *body = json_object();
    if (!body)
    {
        log_message(LOG_ERROR, "Failed to create JSON object for LCD settings");
        return 0;
    }

    json_object_set_new(body, "mode", json_string("image"));
    json_object_set_new(body, "image_file_processed", json_string(image_path));
    json_object_set_new(body, "brightness", json_integer(config->lcd_brightness));
    json_object_set_new(body, "orientation", json_integer(config->lcd_orientation));
    json_object_set_new(body, "colors", json_array());

    char *json_str = json_dumps(body, JSON_COMPACT);
    json_decref(body);
    if (!json_str)
    {
        log_message(LOG_ERROR, "Failed to serialize LCD settings JSON");
        return 0;
    }

    http_response response = {0};
    if (!cc_init_response_buffer(&response, 4096))
    {
        log_message(LOG_ERROR, "Failed to initialize response buffer");
        free(json_str);
        return 0;
    }

    // Configure CURL for JSON PUT request
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(
        cc_session.curl_handle, CURLOPT_WRITEFUNCTION,
        (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, &response);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, cc_session.access_token);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE,
                      &http_response_code);

    int success = check_upload_response(res, http_response_code, &response);

    // Cleanup
    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    free(json_str);
    reset_curl_request_options();

    return success;
}
