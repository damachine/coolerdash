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
 */

#ifndef CC_MAIN_H
#define CC_MAIN_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <stdint.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"

// Basic constants
#define CC_UID_SIZE 128
#define CC_URL_SIZE 512
#define CC_BEARER_HEADER_SIZE (CONFIG_MAX_TOKEN_LEN + 32)

// Maximum safe allocation size to prevent overflow
#define CC_MAX_SAFE_ALLOC_SIZE (SIZE_MAX / 2)

// Forward declarations to reduce compilation dependencies
struct Config;
struct curl_slist;

/** @brief Dynamic buffer for libcurl HTTP response data. */
typedef struct http_response
{
    char *data;
    size_t size;
    size_t capacity;
} http_response;

/** @brief Init HTTP response buffer. */
int cc_init_response_buffer(http_response *response, size_t initial_capacity);

/** @brief Free HTTP response buffer. */
void cc_cleanup_response_buffer(http_response *response);

/** @brief libcurl write callback — appends received data to response buffer. */
size_t write_callback(const void *contents, size_t size, size_t nmemb,
                      http_response *response);

/** @brief Init CURL session; authenticates via Bearer token or Basic Auth. */
int init_coolercontrol_session(const struct Config *config);

/** @brief Returns 1 if session is ready for API calls. */
int is_session_initialized(void);

/** @brief Free CURL resources and close session. */
void cleanup_coolercontrol_session(void);

/** @brief Destroy CURL handle for SIGHUP reload; allows re-init. */
void reset_coolercontrol_session(void);

/** @brief Returns active Bearer token (empty if not set). */
const char *get_session_access_token(void);

/** @brief Upload image to LCD via JSON (CC4) or multipart (CC3). */
int send_image_to_lcd(const struct Config *config, const char *image_path,
                      const char *device_uid);

/** @brief Register shutdown image with CC4; called once at startup. */
int register_shutdown_image_with_cc(const struct Config *config,
                                    const char *image_path,
                                    const char *device_uid);

#endif // CC_MAIN_H
