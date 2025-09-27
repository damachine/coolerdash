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
 * @brief CoolerControl API interface and session management.
 * @details Provides functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API.
 */

// Include necessary headers
#ifndef COOLERCONTROL_H
#define COOLERCONTROL_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <curl/curl.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "config.h"

#define CC_COOKIE_SIZE 512
#define CC_NAME_SIZE 128
#define CC_UID_SIZE 128
#define CC_URL_SIZE 512
#define CC_USERPWD_SIZE 128
#define CC_MAX_SAFE_ALLOC_SIZE (SIZE_MAX / 2)

/**
 * @brief Session state structure.
 * @details Holds the state of the CoolerControl session, including the CURL handle and session initialization status.
 */
struct Config;
struct curl_slist;

typedef struct
{
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;


/**
 * @brief HTTP response buffer structure.
 * @details Holds the state of the CoolerControl session, including the CURL handle and session initialization status.
 */
typedef struct http_response
{
    char *data;
    size_t size;
    size_t capacity;
} http_response;

int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src);
void *cc_secure_malloc(size_t size);
int cc_init_response_buffer(struct http_response *response, size_t initial_capacity);
int cc_validate_response_buffer(const struct http_response *response);
void cc_cleanup_response_buffer(struct http_response *response);
size_t write_callback(char *contents, size_t size, size_t nmemb, void *response);
int init_coolercontrol_session(const struct Config *config);
int is_session_initialized(void);
void cleanup_coolercontrol_session(void);
int get_liquidctl_data(const struct Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);
int init_device_cache(const struct Config *config);
int send_image_to_lcd(const struct Config *config, const char *image_path, const char *device_uid);
const char *extract_device_type_from_json(json_t *dev);

extern CoolerControlSession cc_session;

#endif // COOLERCONTROL_H