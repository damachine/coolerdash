/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CoolerControl API implementation for LCD device communication and sensor data.
 * @details Implements functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API.
 * @example
 *     See function documentation for usage examples.
 */

// Include project headers
#include "../include/coolercontrol.h"
#include "../include/config.h"

// Include necessary headers
#include <jansson.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/stat.h>

/*
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It reallocates the buffer as needed and appends the new data chunk. If memory allocation fails, it frees the buffer and returns 0 to signal an error to libcurl.
 * @example
 *     struct http_response resp = {0};
 *     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
 *     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response) {
    const size_t realsize = size * nmemb;
    const size_t new_size = response->size + realsize + 1;
    
    char *ptr = realloc(response->data, new_size);
    if (!ptr) {
        fprintf(stderr, "Error: realloc failed for response->data\n");
        free(response->data);
        response->data = NULL;
        response->size = 0;
        return 0;
    }
    
    response->data = ptr;
    memcpy(response->data + response->size, contents, realsize);
    response->size += realsize;
    response->data[response->size] = '\0';
    
    return realsize;
}

typedef struct {
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;

static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0
};

/**
 * @brief Initializes the CoolerControl session (CURL setup and login).
 * @details This function initializes the CURL library, sets up the session cookie jar, constructs the login URL and credentials, and performs a login to the CoolerControl API. No handshake or session authentication is performed beyond basic login.
 * @example
 *     if (init_coolercontrol_session(&config)) { ... }
 */
int init_coolercontrol_session(const Config *config) {
    // Initialize cURL and create handle
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle) {
        return 0;
    }

    // Create unique cookie jar path using PID
    int written_cookie = snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar), 
                                  "/tmp/lcd_cookie_%d.txt", getpid());
    if (written_cookie < 0 || (size_t)written_cookie >= sizeof(cc_session.cookie_jar)) {
        cc_session.cookie_jar[sizeof(cc_session.cookie_jar) - 1] = '\0';
    }

    // Configure cookie handling
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR, cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE, cc_session.cookie_jar);

    // Build login URL
    char login_url[CC_URL_SIZE];
    int written_url = snprintf(login_url, sizeof(login_url), "%s/login", config->daemon_address);
    if (written_url < 0 || (size_t)written_url >= sizeof(login_url)) {
        login_url[sizeof(login_url) - 1] = '\0';
    }

    // Build credentials
    char userpwd[CC_USERPWD_SIZE];
    int written_pwd = snprintf(userpwd, sizeof(userpwd), "CCAdmin:%s", config->daemon_password);
    if (written_pwd < 0 || (size_t)written_pwd >= sizeof(userpwd)) {
        userpwd[sizeof(userpwd) - 1] = '\0';
    }

    // Configure cURL for authentication and POST request
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, login_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);

    // Perform login request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    // Check if login was successful
    if (res == CURLE_OK && (response_code == 200 || response_code == 204)) {
        cc_session.session_initialized = 1;
        return 1;
    }

    return 0;
}

/**
 * @brief Sends an image to the LCD display.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT request. The image is specified by its file path, and the device UID is used to identify the target device.
 * @example
 *    send_image_to_lcd(&config, "/opt/coolerdash/images/coolerdash.png", "device_uid");
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid) {
    // Validate input parameters and session state
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized) {
        return 0;
    }
    
    // Construct upload URL
    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url), "%s/devices/%s/settings/lcd/lcd/images", 
             config->daemon_address, device_uid);
    
    // Initialize multipart form
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    if (!form) return 0;
    
    curl_mimepart *field;
    
    // Add mode field
    field = curl_mime_addpart(form);
    curl_mime_name(field, "mode");
    curl_mime_data(field, "image", CURL_ZERO_TERMINATED);
    
    // Add brightness field
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "brightness");
    curl_mime_data(field, brightness_str, CURL_ZERO_TERMINATED);
    
    // Add orientation field
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "orientation");
    curl_mime_data(field, orientation_str, CURL_ZERO_TERMINATED);
    
    // Add image file
    field = curl_mime_addpart(form);
    curl_mime_name(field, "images[]");
    curl_mime_filedata(field, image_path);
    curl_mime_type(field, "image/png");
    
    // Configure curl options
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    
    // Perform request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    
    // Get HTTP response code
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);

    // Validate response code is within valid HTTP range
    if (http_response_code < 100 || http_response_code > 599) {
        fprintf(stderr, "Invalid HTTP response code: %ld\n", http_response_code);
        return CC_ERROR_INVALID_RESPONSE;
    }
    
    // Cleanup
    curl_mime_free(form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    
    // Return success status
    return (res == CURLE_OK && http_response_code == 200);
}

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details Frees all resources and closes the session. This includes cleaning up the CURL handle and deleting the cookie jar file.
 * @example
 *     cleanup_coolercontrol_session();
 */
void cleanup_coolercontrol_session(void) {
    static int cleanup_done = 0;
    if (cleanup_done) return;
    
    int all_cleaned = 1;
    
    // Clean up CURL handle
    if (cc_session.curl_handle) {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }
    
    // Perform global CURL cleanup
    curl_global_cleanup();
    
    // Remove cookie jar file
    if (unlink(cc_session.cookie_jar) != 0) {
        all_cleaned = 0;
    }
    
    // Mark session as uninitialized
    cc_session.session_initialized = 0;
    
    // Set cleanup flag only if all operations succeeded
    if (all_cleaned) {
        cleanup_done = 1;
    }
}

int is_session_initialized(void) {
    return cc_session.session_initialized;
}
