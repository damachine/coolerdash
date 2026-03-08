/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Session management, authentication and LCD image upload via CoolerControl REST API.
 */

#define _POSIX_C_SOURCE 200112L

// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

#include "../device/config.h"
#include "cc_conf.h"
#include "cc_main.h"

/** Allocate and initialise an HTTP response buffer. */
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

/** Free an HTTP response buffer and reset its state. */
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

/** CoolerControl session state (CURL handle, init flag). */
typedef struct
{
    CURL *curl_handle;
    int session_initialized;
} CoolerControlSession;

/** Global session instance. */
static CoolerControlSession cc_session = {
    .curl_handle = NULL, .session_initialized = 0};

/** Grow response buffer using exponential strategy. */
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

/** libcurl write callback — appends received data to an http_response buffer. */
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

/** Check snprintf result for truncation. */
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
 * @brief Apply TLS verification options to a CURL handle.
 * @details Uses tls_skip_verify flag for self-signed certs; verifies peer
 * when daemon_address starts with "https://".
 */
void cc_apply_tls_to_curl(CURL *curl, const Config *config)
{
    if (!curl || !config)
        return;
    if (config->tls_skip_verify)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    else if (strncmp(config->daemon_address, "https://", 8) == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }
}

/**
 * @brief Append Authorization Bearer header if access_token is configured.
 * @details Returns the (possibly extended) header list.
 */
struct curl_slist *cc_apply_auth_to_curl(struct curl_slist *headers,
                                         const Config *config)
{
    if (!config || config->access_token[0] == '\0')
        return headers;

    char auth_header[CC_AUTH_HEADER_SIZE];
    int written = snprintf(auth_header, sizeof(auth_header),
                           "Authorization: Bearer %s", config->access_token);
    if (written > 0 && (size_t)written < sizeof(auth_header))
        headers = curl_slist_append(headers, auth_header);
    return headers;
}

/**
 * @brief Initialise a CoolerControl session (CURL setup + Bearer token auth).
 */
int init_coolercontrol_session(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle)
        return 0;

    cc_session.session_initialized = 1;
    log_message(LOG_STATUS, "CoolerControl session initialized (Bearer token auth)");
    return 1;
}

/** Returns 1 if the CoolerControl session is ready for use. */
int is_session_initialized(void) { return cc_session.session_initialized; }

/** Release all CURL resources. */
void cleanup_coolercontrol_session(void)
{
    static int cleanup_done = 0;
    if (cleanup_done)
        return;

    if (cc_session.curl_handle)
    {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }

    curl_global_cleanup();

    cc_session.session_initialized = 0;
    cleanup_done = 1;
}

/** Return 1 if parameters and session state are valid for an LCD upload. */
static int validate_upload_params(const char *image_path, const char *device_uid)
{
    if (!cc_session.curl_handle || !image_path || !device_uid ||
        !cc_session.session_initialized)
    {
        log_message(LOG_ERROR, "Invalid parameters or session not initialized");
        return 0;
    }
    return 1;
}

/** Return 1 if the CURL result and HTTP code indicate a successful upload. */
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
 * @brief CC 4.0: PUT JSON body with image_file_processed path to the LCD settings endpoint.
 * @details CoolerControl reads the file directly — no multipart serialisation needed.
 */
static int send_image_via_json(const Config *config, const char *image_path,
                               const char *device_uid)
{
    char upload_url[CC_URL_SIZE];
    int written = snprintf(upload_url, sizeof(upload_url),
                           "%s/devices/%s/settings/%s/lcd?log=false",
                           config->daemon_address, device_uid,
                           config->channel_name);
    if (!validate_snprintf(written, sizeof(upload_url), upload_url))
        return 0;

    /* Build JSON body - path is a plain filesystem path, no special chars */
    char json_body[1024];
    written = snprintf(json_body, sizeof(json_body),
                       "{\"mode\":\"image\",\"brightness\":%d,"
                       "\"orientation\":%d,"
                       "\"image_file_processed\":\"%s\",\"colors\":[]}",
                       config->lcd_brightness, config->lcd_orientation,
                       image_path);
    if (!validate_snprintf(written, sizeof(json_body), json_body))
        return 0;

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDSIZE,
                     (long)strlen(json_body));

    cc_apply_tls_to_curl(cc_session.curl_handle, config);

    http_response response = {0};
    if (!cc_init_response_buffer(&response, 4096))
        return 0;

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION,
                     (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, &response);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = cc_apply_auth_to_curl(headers, config);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long http_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_code);

    int success = check_upload_response(res, http_code, &response);

    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDSIZE, 0L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);

    return success;
}

/**
 * @brief Upload a PNG image to the device LCD via JSON body (CC 4.0).
 */
int send_image_to_lcd(const Config *config, const char *image_path,
                      const char *device_uid)
{
    if (!validate_upload_params(image_path, device_uid))
        return 0;

    return send_image_via_json(config, image_path, device_uid);
}

/**
 * @brief Blocking wrapper for `send_image_to_lcd` with timeout and retries.
 * @details Temporarily sets CURLOPT_TIMEOUT and CURLOPT_CONNECTTIMEOUT on the
 * global CURL handle, attempts the upload up to `retries` times and restores
 * timeouts to defaults after completion.
 */
int send_image_to_lcd_blocking(const Config *config, const char *image_path,
                               const char *device_uid, int timeout_seconds,
                               int retries)
{
    if (!validate_upload_params(image_path, device_uid))
        return 0;

    if (timeout_seconds <= 0)
        timeout_seconds = 5; // sensible default
    if (retries <= 0)
        retries = 1;

    int attempt;
    int success = 0;

    for (attempt = 0; attempt < retries; attempt++)
    {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_TIMEOUT,
                         (long)timeout_seconds);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_CONNECTTIMEOUT,
                         (long)(timeout_seconds / 2 > 0 ? timeout_seconds / 2 : 1));

        success = send_image_to_lcd(config, image_path, device_uid);
        if (success)
            break;

        log_message(LOG_WARNING, "Shutdown upload attempt %d/%d failed",
                    attempt + 1, retries);
    }

    /* restore: no timeout (default) */
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CONNECTTIMEOUT, 0L);

    return success;
}
