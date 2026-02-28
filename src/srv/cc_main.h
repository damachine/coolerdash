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

#ifndef CC_MAIN_H
#define CC_MAIN_H

// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <stdint.h>
#include <curl/curl.h>
// cppcheck-suppress-end missingIncludeSystem

#include "../device/config.h"
#define CC_COOKIE_SIZE 512
#define CC_UID_SIZE 128
#define CC_URL_SIZE 512
#define CC_USERPWD_SIZE 128
#define CC_AUTH_HEADER_SIZE 320 /**< "Authorization: Bearer " (24) + token (256) + null */

// Maximum safe allocation size to prevent overflow
#define CC_MAX_SAFE_ALLOC_SIZE (SIZE_MAX / 2)

// Forward declarations to reduce compilation dependencies
struct Config;

/** Dynamic response buffer for libcurl HTTP operations. */
typedef struct http_response
{
    char *data;
    size_t size;
    size_t capacity;
} http_response;

/** Allocate and initialise an HTTP response buffer. */
int cc_init_response_buffer(http_response *response, size_t initial_capacity);

/** Free an HTTP response buffer and reset its state. */
void cc_cleanup_response_buffer(http_response *response);

/** libcurl write callback — appends received data to an http_response buffer. */
size_t write_callback(const void *contents, size_t size, size_t nmemb,
                      http_response *response);

/**
 * @brief Initialise CURL session and authenticate with the CoolerControl daemon.
 * @details CC 4.0 (access_token set): skips login, uses Bearer token.
 *          CC 3.x: performs Basic Auth login and stores session cookie.
 */
int init_coolercontrol_session(const struct Config *config);

/** Returns 1 if the CoolerControl session is ready for use. */
int is_session_initialized(void);

/** Release all CURL resources and remove the session cookie file. */
void cleanup_coolercontrol_session(void);

/**
 * @brief Upload a PNG image to the device LCD.
 * @details CC 4.0: JSON body with image_file_processed path.
 *          CC 3.x: multipart PUT to /lcd/images.
 */
int send_image_to_lcd(const struct Config *config, const char *image_path,
                      const char *device_uid);

/**
 * @brief Blocking variant of send_image_to_lcd with timeout and retry support.
 * @details Sets CURLOPT_TIMEOUT/CURLOPT_CONNECTTIMEOUT for the duration of
 * the upload, then restores defaults. Used on the shutdown path.
 */
int send_image_to_lcd_blocking(const struct Config *config,
                               const char *image_path,
                               const char *device_uid,
                               int timeout_seconds,
                               int retries);

/**
 * @brief Configure SSL verification on a CURL handle.
 * @details Disables peer/host check when tls_skip_verify is set;
 * enables full verification for https:// addresses.
 */
void cc_apply_tls_to_curl(CURL *curl, const struct Config *config);

/**
 * @brief Append an Authorization: Bearer header to a curl_slist if a token is configured.
 * @details Returns the (possibly extended) list; caller owns it.
 */
struct curl_slist *cc_apply_auth_to_curl(struct curl_slist *headers,
                                         const struct Config *config);

/**
 * @brief Register the shutdown image with CoolerControl (CC 4.0, one-shot on startup).
 * @details PUT /devices/{uid}/settings/{channel}/lcd/shutdown-image.
 * No-op if paths_image_shutdown does not exist.
 * @return 1 on success, 0 on failure or missing file
 */
int register_shutdown_image(const struct Config *config, const char *device_uid);

#endif // CC_MAIN_H
