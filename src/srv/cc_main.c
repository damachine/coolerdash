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

/** CoolerControl session state (CURL handle, cookie jar, init flag). */
typedef struct
{
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;

/** Global session instance. */
static CoolerControlSession cc_session = {
    .curl_handle = NULL, .cookie_jar = {0}, .session_initialized = 0};

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

/** Build login URL and Basic-Auth credentials from config. */
static int build_login_credentials(const Config *config, char *login_url,
                                   size_t url_size, char *userpwd,
                                   size_t pwd_size)
{
    int written_url =
        snprintf(login_url, url_size, "%s/login", config->daemon_address);
    if (!validate_snprintf(written_url, url_size, login_url))
        return 0;

    int written_pwd =
        snprintf(userpwd, pwd_size, "CCAdmin:%s", config->daemon_password);
    if (!validate_snprintf(written_pwd, pwd_size, userpwd))
        return 0;

    return 1;
}

/** Configure CURL for the CC 3.x login request. */
static struct curl_slist *configure_login_curl(CURL *curl, const Config *config,
                                               const char *login_url,
                                               const char *userpwd)
{
    curl_easy_setopt(curl, CURLOPT_URL, login_url);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = cc_apply_auth_to_curl(headers, config);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    cc_apply_tls_to_curl(curl, config);

    return headers;
}

/** Return 1 if the CURL response indicates a successful login. */
static int is_login_successful(CURLcode res, long response_code)
{
    return (res == CURLE_OK && (response_code == 200 || response_code == 204));
}

/**
 * @brief Initialise a CoolerControl session (CURL setup + authentication).
 * @details CC 4.0 (access_token set): skips login, uses Bearer token.
 *          CC 3.x: performs Basic Auth login and stores session cookie.
 */
int init_coolercontrol_session(const Config *config)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle)
        return 0;

    /* CC 4.0 path: Bearer token auth — skip login endpoint and cookie jar */
    if (config->access_token[0] != '\0')
    {
        cc_session.cookie_jar[0] = '\0'; /* no cookie jar in token mode */
        cc_session.session_initialized = 1;
        log_message(LOG_STATUS, "CoolerControl session initialized (Bearer token auth)");
        return 1;
    }

    /* CC 3.x path: Basic Auth + session cookie */
    int written_cookie =
        snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar),
                 "/tmp/coolerdash_cookie_%d.txt", getpid());
    if (written_cookie < 0 ||
        (size_t)written_cookie >= sizeof(cc_session.cookie_jar))
    {
        cc_session.cookie_jar[sizeof(cc_session.cookie_jar) - 1] = '\0';
    }

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR,
                     cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE,
                     cc_session.cookie_jar);

    char login_url[CC_URL_SIZE];
    char userpwd[CC_USERPWD_SIZE];
    if (!build_login_credentials(config, login_url, sizeof(login_url), userpwd,
                                 sizeof(userpwd)))
        return 0;

    struct curl_slist *headers =
        configure_login_curl(cc_session.curl_handle, config, login_url, userpwd);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE,
                      &response_code);

    memset(userpwd, 0, sizeof(userpwd));

    if (headers)
        curl_slist_free_all(headers);

    if (is_login_successful(res, response_code))
    {
        cc_session.session_initialized = 1;
        return 1;
    }

    log_message(LOG_ERROR, "Login failed: CURL code %d, HTTP code %ld", res,
                response_code);
    return 0;
}

/** Returns 1 if the CoolerControl session is ready for use. */
int is_session_initialized(void) { return cc_session.session_initialized; }

/** Release all CURL resources and remove the session cookie file. */
void cleanup_coolercontrol_session(void)
{
    static int cleanup_done = 0;
    if (cleanup_done)
        return;

    int all_cleaned = 1;

    if (cc_session.curl_handle)
    {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }

    curl_global_cleanup();

    if (cc_session.cookie_jar[0] != '\0' && unlink(cc_session.cookie_jar) != 0)
    {
        all_cleaned = 0;
    }

    cc_session.session_initialized = 0;

    if (all_cleaned)
    {
        cleanup_done = 1;
    }
}

/** Add a named string field to a curl_mime multipart form. */
static int add_mime_field(curl_mime *form, const char *field_name,
                          const char *field_value)
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
        log_message(LOG_ERROR, "Failed to set %s field name: %s", field_name,
                    curl_easy_strerror(result));
        return 0;
    }

    result = curl_mime_data(field, field_value, CURL_ZERO_TERMINATED);
    if (result != CURLE_OK)
    {
        log_message(LOG_ERROR, "Failed to set %s field data: %s", field_name,
                    curl_easy_strerror(result));
        return 0;
    }

    return 1;
}

/** Add the PNG image file field to a curl_mime multipart form. */
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
        log_message(LOG_ERROR, "Failed to set image field name: %s",
                    curl_easy_strerror(result));
        return 0;
    }

    result = curl_mime_filedata(field, image_path);
    if (result != CURLE_OK)
    {
        struct stat file_stat;
        if (stat(image_path, &file_stat) == 0)
        {
            log_message(LOG_ERROR, "Failed to set image file data: %s",
                        curl_easy_strerror(result));
        }
        return 0;
    }

    result = curl_mime_type(field, "image/png");
    if (result != CURLE_OK)
    {
        log_message(LOG_ERROR, "Failed to set image MIME type: %s",
                    curl_easy_strerror(result));
        return 0;
    }

    return 1;
}

/** Build the complete multipart form for an LCD image upload. */
static curl_mime *build_lcd_upload_form(const Config *config,
                                        const char *image_path)
{
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    if (!form)
    {
        log_message(LOG_ERROR, "Failed to initialize multipart form");
        return NULL;
    }

    if (!add_mime_field(form, "mode", "image"))
    {
        curl_mime_free(form);
        return NULL;
    }

    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d",
             config->lcd_brightness);
    if (!add_mime_field(form, "brightness", brightness_str))
    {
        curl_mime_free(form);
        return NULL;
    }

    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d",
             config->lcd_orientation);
    if (!add_mime_field(form, "orientation", orientation_str))
    {
        curl_mime_free(form);
        return NULL;
    }

    if (!add_image_file_field(form, image_path))
    {
        curl_mime_free(form);
        return NULL;
    }

    return form;
}

/** Set up CURL options for a multipart LCD image upload request. */
static struct curl_slist *configure_lcd_upload_curl(const Config *config,
                                                    const char *upload_url,
                                                    curl_mime *form,
                                                    http_response *response)
{
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");

    cc_apply_tls_to_curl(cc_session.curl_handle, config);

    curl_easy_setopt(
        cc_session.curl_handle, CURLOPT_WRITEFUNCTION,
        (size_t (*)(const void *, size_t, size_t, void *))write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, response);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = cc_apply_auth_to_curl(headers, config);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);

    return headers;
}

/** Reset all CURL options set during an LCD image upload. */
static void cleanup_lcd_upload_curl(void)
{
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);
}

/** Validate parameters and session state before an LCD upload. */
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
                           "%s/devices/%s/settings/%s/lcd",
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

/** Multipart PUT to an arbitrary LCD endpoint URL (shared by CC 3.x and shutdown-image paths). */
static int send_image_multipart_to_url(const Config *config,
                                       const char *image_path,
                                       const char *url)
{
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

    struct curl_slist *headers =
        configure_lcd_upload_curl(config, url, form, &response);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE,
                      &http_response_code);

    int success = check_upload_response(res, http_response_code, &response);

    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    curl_mime_free(form);
    cleanup_lcd_upload_curl();

    return success;
}

/**
 * @brief Upload a PNG image to the device LCD.
 * @details CC 4.0 (access_token set): JSON body via send_image_via_json.
 *          CC 3.x (no token): multipart PUT to /lcd/images.
 */
int send_image_to_lcd(const Config *config, const char *image_path,
                      const char *device_uid)
{
    if (!validate_upload_params(image_path, device_uid))
        return 0;

    /* CC 4.0: JSON body, CC reads file directly — no multipart upload */
    if (config->access_token[0] != '\0')
        return send_image_via_json(config, image_path, device_uid);

    /* CC 3.x: multipart PUT to /lcd/images */
    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url),
             "%s/devices/%s/settings/%s/lcd/images?log=false",
             config->daemon_address, device_uid, config->channel_name);

    return send_image_multipart_to_url(config, image_path, upload_url);
}

/**
 * @brief Register a persistent LCD shutdown image with CoolerControl (CC 4.0).
 * @details Uploads the shutdown PNG once at startup via PUT
 * /devices/{uid}/settings/{channel}/lcd/shutdown-image.
 * CoolerControl stores and auto-applies it on daemon shutdown.
 * @return 1 on success, 0 on failure or missing image file
 */
int register_shutdown_image(const Config *config, const char *device_uid)
{
    if (!config || !device_uid || !cc_session.session_initialized)
        return 0;

    /* Check that the shutdown image exists */
    struct stat st;
    if (stat(config->paths_image_shutdown, &st) != 0)
    {
        log_message(LOG_INFO,
                    "No shutdown image at %s — skipping CC shutdown registration",
                    config->paths_image_shutdown);
        return 0;
    }

    char url[CC_URL_SIZE];
    int written = snprintf(url, sizeof(url),
                           "%s/devices/%s/settings/%s/lcd/shutdown-image",
                           config->daemon_address, device_uid,
                           config->channel_name);
    if (!validate_snprintf(written, sizeof(url), url))
        return 0;

    int success = send_image_multipart_to_url(config,
                                              config->paths_image_shutdown, url);
    if (success)
        log_message(LOG_STATUS,
                    "Shutdown image registered with CoolerControl (persistent)");
    else
        log_message(LOG_WARNING, "Failed to register shutdown image with CoolerControl");

    return success;
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
