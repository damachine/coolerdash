/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CoolerControl API implementation for LCD device communication.
 * @details Implements functions for initializing, authenticating, and communicating with CoolerControl LCD devices.
 * @example
 *     See function documentation for usage examples.
 */

// Buffer size constants
#define CC_UID_SIZE      128
#define CC_NAME_SIZE     128
#define CC_COOKIE_SIZE   256
#define CC_URL_SIZE      256
#define CC_USERPWD_SIZE  128
#define CC_DEVICE_SECTION_SIZE 4096

// Include project headers
#include "../include/coolercontrol.h"
#include "../include/config.h"

// Include necessary headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <curl/curl.h>

/**
 * @brief Response buffer structure for HTTP responses.
 * @details Used to store dynamically allocated HTTP response data and its size for cURL operations.
 * @example
 *     struct http_response resp = {0};
 *     resp.data = malloc(1);
 *     resp.size = 0;
 *     // Use with cURL: curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
 */
struct http_response {
    char *data;
    size_t size;
};

/**
 * @brief Callback function to write HTTP response data.
 * @details Writes received HTTP response data into a dynamically growing buffer.
 * @example
 *     struct http_response resp = {0};
 *     resp.data = malloc(1);
 *     resp.size = 0;
 *     // ...
 *     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
 *     curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
 */
int init_coolercontrol_session(const Config *config);
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid);
int upload_image_to_device(const Config *config, const char* image_path, const char* device_uid);
void cleanup_coolercontrol_session(void);
int is_session_initialized(void);
int get_device_name(const Config *config, char* name_buffer, size_t buffer_size);
int get_device_uid(const Config *config, char* uid_buffer, size_t buffer_size);
int init_cached_device_uid(const Config *config);

static size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "[CoolerDash] Error: realloc failed for response->data\n");
        if (response->data) {
            free(response->data);
            response->data = NULL;
        }
        return 0;  // Out of memory
    }
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;  // Null-terminate
    return realsize;
}

/**
 * @brief Session state struct for CoolerControl API.
 * @details Encapsulates all internal session variables (cURL handle, cookie jar, session state, cached UID) for safe and modular access. Avoids global variables and improves maintainability.
 * @example
 *     static CoolerControlSession cc_session = {0};
 */
typedef struct {
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
    char cached_device_uid[CC_UID_SIZE];
} CoolerControlSession;

static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0,
    .cached_device_uid = {0}
};

/**
 * @brief Initializes cURL and authenticates with the CoolerControl daemon using configuration.
 * @details Sets up the cURL handle and logs in to the CoolerControl daemon using basic authentication.
 * @example
 *     if (!init_coolercontrol_session(&config)) {
 *         // handle error
 *     }
 */
int init_coolercontrol_session(const Config *config) {
    curl_global_init(CURL_GLOBAL_DEFAULT); 
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle) return 0;
    int written_cookie = snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar), "/tmp/lcd_cookie_%d.txt", getpid());
    if (written_cookie < 0 || (size_t)written_cookie >= sizeof(cc_session.cookie_jar)) cc_session.cookie_jar[sizeof(cc_session.cookie_jar)-1] = '\0';
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR, cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE, cc_session.cookie_jar);
    char login_url[CC_URL_SIZE];
    int written_url = snprintf(login_url, sizeof(login_url), "%s/login", config->daemon_address);
    if (written_url < 0 || (size_t)written_url >= sizeof(login_url)) login_url[sizeof(login_url)-1] = '\0';
    char userpwd[CC_USERPWD_SIZE];
    int written_pwd = snprintf(userpwd, sizeof(userpwd), "CCAdmin:%s", config->daemon_password);
    if (written_pwd < 0 || (size_t)written_pwd >= sizeof(userpwd)) userpwd[sizeof(userpwd)-1] = '\0';
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, login_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL); // Ignore response
    
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (res == CURLE_OK && (response_code == 200 || response_code == 204)) {
        cc_session.session_initialized = 1;
        return 1;
    }
    
    return 0;
}

/**
 * @brief Sends an image directly to the LCD of the CoolerControl device.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT request.
 * @example
 *     send_image_to_lcd(&config, "/opt/coolerdash/images/coolerdash.png", uid);
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid) {
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized) return 0;

    // URL for LCD image upload
    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url), 
             "%s/devices/%s/settings/lcd/lcd/images", config->daemon_address, device_uid);
    
    // Determine MIME type
    const char* mime_type = "image/png";

    // Create multipart form
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    curl_mimepart *field;
    
    // mode field
    field = curl_mime_addpart(form);
    curl_mime_name(field, "mode");
    curl_mime_data(field, "image", CURL_ZERO_TERMINATED);
    
    // brightness field
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "brightness");
    curl_mime_data(field, brightness_str, CURL_ZERO_TERMINATED);
    
    // orientation field
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "orientation");
    curl_mime_data(field, orientation_str, CURL_ZERO_TERMINATED);
    
    // images[] field (the actual image)
    field = curl_mime_addpart(form);
    curl_mime_name(field, "images[]");
    curl_mime_filedata(field, image_path);
    curl_mime_type(field, mime_type);
    
    // Configure cURL
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    
    // Execute request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    
    // Cleanup
    curl_mime_free(form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    
    return (res == CURLE_OK && response_code == 200);
}

/**
 * @brief Alias function for send_image_to_lcd (for better API compatibility).
 * @details Calls send_image_to_lcd for API compatibility.
 * @example
 *     upload_image_to_device(&config, "/opt/coolerdash/images/coolerdash.png", uid);
 */
int upload_image_to_device(const Config *config, const char* image_path, const char* device_uid) {
    return send_image_to_lcd(config, image_path, device_uid);
}

/**
 * @brief Terminates the CoolerControl session and cleans up.
 * @details Frees all resources, cleans up cURL, and removes the session cookie file.
 * @example
 *     cleanup_coolercontrol_session();
 */
void cleanup_coolercontrol_session(void) {
    static int cleanup_done = 0;
    if (cleanup_done) return;
    int all_cleaned = 1;
    if (cc_session.curl_handle) {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }
    curl_global_cleanup();
    if (unlink(cc_session.cookie_jar) != 0) {
        all_cleaned = 0;
    }
    cc_session.session_initialized = 0;
    if (all_cleaned) cleanup_done = 1;
}

/**
 * @brief Returns whether the session is initialized.
 * @details Checks if the session is ready for communication.
 * @example
 *     if (is_session_initialized()) {
 *         // session is ready
 *     }
 */
int is_session_initialized(void) {
    return cc_session.session_initialized;
}

/**
 * @brief Extracts the value of a JSON string field from a device section.
 * @details Searches for a field like "name":"..." or "uid":"..." and copies its value.
 * @example
 *     char name[128];
 *     extract_json_string_field(device_section, "name", name, sizeof(name));
 */
static int extract_json_string_field(const char *section, const char *key, char *buffer, size_t bufsize)
{
    if (!section || !key || !buffer || bufsize == 0) return 0;
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    char *pos = strstr(section, pattern);
    if (!pos) return 0;
    pos += strlen(pattern);
    char *end = strchr(pos, '"');
    if (!end) return 0;
    size_t len = end - pos;
    if (len >= bufsize) len = bufsize - 1;
    strncpy(buffer, pos, len);
    buffer[len] = '\0';
    return 1;
}

/**
 * @brief Parses the first Liquidctl device and extracts name and/or uid.
 * @details Parses the device list and extracts the name and/or uid of the first Liquidctl device.
 * @example
 *     char name[128], uid[128];
 *     parse_device_fields(&config, name, sizeof(name), uid, sizeof(uid));
 */
static int parse_device_fields(const Config *config, char* name_buffer, size_t name_size, char* uid_buffer, size_t uid_size)
{
    struct http_response response = {0};
    response.data = malloc(1);
    if (!response.data) return 0;
    response.size = 0;

    char devices_url[CC_URL_SIZE];
    snprintf(devices_url, sizeof(devices_url), "%.*s/devices", (int)(sizeof(devices_url) - 9), config->daemon_address);

    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, devices_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);

    int found = 0;
    if (res == CURLE_OK && response_code == 200 && response.data) {
        char *search_pos = response.data;
        while ((search_pos = strstr(search_pos, "\"uid\":\"")) != NULL) {
            char *device_start = search_pos;
            while (device_start > response.data && *device_start != '{') device_start--;
            char *device_end = strchr(search_pos, '}');
            if (!device_end) device_end = response.data + response.size;
            char device_section[CC_DEVICE_SECTION_SIZE];
            size_t section_length = device_end - device_start;
            if (section_length < sizeof(device_section)) {
                memcpy(device_section, device_start, section_length);
                device_section[section_length] = '\0';
                if (strstr(device_section, "\"type\":\"Liquidctl\"")) {
                    int name_ok = 0, uid_ok = 0;
                    if (name_buffer && name_size > 0)
                        name_ok = extract_json_string_field(device_section, "name", name_buffer, name_size);
                    if (uid_buffer && uid_size > 0)
                        uid_ok = extract_json_string_field(device_section, "uid", uid_buffer, uid_size);
                    if ((name_buffer && name_ok) || (uid_buffer && uid_ok)) {
                        found = 1;
                        break;
                    }
                }
            }
            search_pos = device_end;
        }
    }
    if (response.data) free(response.data);
    return found;
}

/**
 * @brief Retrieves the full name of the LCD device.
 * @details Queries the CoolerControl API for the device name and writes it to the provided buffer.
 * @example
 *     char name[128];
 *     if (get_device_name(&config, name, sizeof(name))) {
 *         // use name
 *     }
 */
int get_device_name(const Config *config, char* name_buffer, size_t buffer_size) {
    if (!cc_session.curl_handle || !name_buffer || buffer_size == 0 || !cc_session.session_initialized) return 0;
    return parse_device_fields(config, name_buffer, buffer_size, NULL, 0);
}

/**
 * @brief Retrieves the UID of the first LCD device found.
 * @details Queries the CoolerControl API for the device UID and writes it to the provided buffer.
 * @example
 *     char uid[128];
 *     if (get_device_uid(&config, uid, sizeof(uid))) {
 *         // use uid
 *     }
 */
int get_device_uid(const Config *config, char* uid_buffer, size_t buffer_size) {
    if (!cc_session.curl_handle || !uid_buffer || buffer_size == 0 || !cc_session.session_initialized) return 0;
    return parse_device_fields(config, NULL, 0, uid_buffer, buffer_size);
}

/**
 * @brief Initialize and cache the device UID at program start.
 * @details This function must be called once after session initialization.
 * @example
 *     if (!init_cached_device_uid(&config)) { ... }
 */
int init_cached_device_uid(const Config *config) {
    if (!get_device_uid(config, cc_session.cached_device_uid, sizeof(cc_session.cached_device_uid)) || !cc_session.cached_device_uid[0]) {
        fprintf(stderr, "[CoolerDash] Error: Could not detect LCD device UID!\n");
        return 0;
    }
    return 1;
}

/**
 * @brief Get the cached device UID (read-only).
 * @details Returns a pointer to the cached UID string (empty string if not initialized).
 * @example
 *     const char* uid = get_cached_device_uid();
 *     if (uid[0]) { ... }
 */
const char* get_cached_device_uid(void) {
    return cc_session.cached_device_uid;
}
