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
#include <jansson.h>

char g_last_device_uid[64] = {0};

struct http_response {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response) {
    /*
     * @brief Callback for libcurl to write received data into a buffer.
     * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It reallocates the buffer as needed and appends the new data chunk. If memory allocation fails, it frees the buffer and returns 0 to signal an error to libcurl.
     * @example
     *     struct http_response resp = {0};
     *     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
     *     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
     */
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "[CoolerDash] Error: realloc failed for response->data\n");
        if (response->data) {
            free(response->data);
            response->data = NULL;
        }
        return 0;
    }
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
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
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if (res == CURLE_OK && (response_code == 200 || response_code == 204)) {
        cc_session.session_initialized = 1;
        return 1;
    }
    return 0;
}

int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid) {
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized) return 0;
    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url), "%s/devices/%s/settings/lcd/lcd/images", config->daemon_address, device_uid);
    const char* mime_type = "image/png";
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    curl_mimepart *field;
    field = curl_mime_addpart(form);
    curl_mime_name(field, "mode");
    curl_mime_data(field, "image", CURL_ZERO_TERMINATED);
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "brightness");
    curl_mime_data(field, brightness_str, CURL_ZERO_TERMINATED);
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "orientation");
    curl_mime_data(field, orientation_str, CURL_ZERO_TERMINATED);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "images[]");
    curl_mime_filedata(field, image_path);
    curl_mime_type(field, mime_type);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_mime_free(form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    return (res == CURLE_OK && response_code == 200);
}

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

int is_session_initialized(void) {
    return cc_session.session_initialized;
}

int cc_get_sensor_data(const Config *config, cc_sensor_data_t *data)
{
    if (!config || !data) return 0;
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    char url[CC_URL_SIZE];
    snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    struct http_response chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL); // Header wird gleich gesetzt
    // Setze Content-Type und Accept Header
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    // Sende Body wie in der API-Doku (all: false)
    const char *post_data = "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    int result = 0;
    float temp_1 = 0.0f;
    float temp_2 = 0.0f;
    if (curl_easy_perform(curl) == CURLE_OK) {
        json_t *root = json_loads(chunk.data, 0, NULL);
        if (root) {
            json_t *devices = json_object_get(root, "devices");
            if (devices && json_is_array(devices)) {
                size_t i;
                json_t *dev;
                int found_lcd = 0;
                float temp_1_found = 0.0f;
                float temp_2_found = 0.0f;
                json_array_foreach(devices, i, dev) {
                    json_t *type_val = json_object_get(dev, "type");
                    // LCD-Device UID Extract
                    if (type_val && json_is_string(type_val) && strcmp(json_string_value(type_val), "Liquidctl") == 0) {
                        json_t *uid_val = json_object_get(dev, "uid");
                        if (uid_val && json_is_string(uid_val)) {
                            strncpy(data->device_uid, json_string_value(uid_val), CC_UID_SIZE-1);
                            data->device_uid[CC_UID_SIZE-1] = '\0';
                            found_lcd = 1;
                        }
                    }
                    // Extract temp_1 (CPU) specifically from the device of type "CPU"
                    // Looks for the latest status entry and searches for the "temp1" sensor value
                    if (type_val && json_is_string(type_val) && strcmp(json_string_value(type_val), "CPU") == 0) {
                        json_t *status_history = json_object_get(dev, "status_history");
                        if (status_history && json_is_array(status_history) && json_array_size(status_history) > 0) {
                            json_t *last_status = json_array_get(status_history, json_array_size(status_history)-1);
                            if (last_status) {
                                json_t *temps = json_object_get(last_status, "temps");
                                if (temps && json_is_array(temps)) {
                                    size_t j;
                                    json_t *temp_entry;
                                    json_array_foreach(temps, j, temp_entry) {
                                        json_t *name_val = json_object_get(temp_entry, "name");
                                        json_t *temp_val = json_object_get(temp_entry, "temp");
                                        if (name_val && json_is_string(name_val) && temp_val && json_is_number(temp_val)) {
                                            const char *sensor_name = json_string_value(name_val);
                                            if (strcmp(sensor_name, "temp1") == 0) {
                                                temp_1_found = (float)json_number_value(temp_val);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // Extract temp_2 (GPU) specifically from the device of type "GPU"
                    if (type_val && json_is_string(type_val) && strcmp(json_string_value(type_val), "GPU") == 0) {
                        json_t *status_history = json_object_get(dev, "status_history");
                        if (status_history && json_is_array(status_history) && json_array_size(status_history) > 0) {
                            json_t *last_status = json_array_get(status_history, json_array_size(status_history)-1);
                            if (last_status) {
                                json_t *temps = json_object_get(last_status, "temps");
                                if (temps && json_is_array(temps)) {
                                    size_t j;
                                    json_t *temp_entry;
                                    json_array_foreach(temps, j, temp_entry) {
                                        json_t *name_val = json_object_get(temp_entry, "name");
                                        json_t *temp_val = json_object_get(temp_entry, "temp");
                                        if (name_val && json_is_string(name_val) && temp_val && json_is_number(temp_val)) {
                                            const char *sensor_name = json_string_value(name_val);
                                            if (strstr(sensor_name, "GPU") || strstr(sensor_name, "gpu")) {
                                                temp_2_found = (float)json_number_value(temp_val);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                temp_1 = temp_1_found;
                temp_2 = temp_2_found;
                // Set UID only if LCD was found
                if (found_lcd) result = 1;
                else result = 0;
            }
            json_decref(root);
        }
    }
    // Set values for display image
    data->temp_1 = temp_1;
    data->temp_2 = temp_2;
    // device_uid remains unchanged
    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

/**
 * @brief Initialize monitor subsystem (CPU/GPU sensors).
 * @details Stub: No hardware-specific initialization required for API-based sensors.
 */
int monitor_init(const Config *config) {
    (void)config;
    return 1;
}

/**
 * @brief Get current temperatures from all available sensors.
 * @details Reads the current CPU and GPU temperatures via API. Returns 1 on success, 0 on failure.
 */
int monitor_get_temperatures(float *temp_1, float *temp_2) {
    cc_sensor_data_t data = {0};
    // Hier ggf. Konfigurationszeiger global oder als Parameter nutzen
    extern const Config *g_config_ptr;
    if (!g_config_ptr) return 0;
    if (!cc_get_sensor_data(g_config_ptr, &data)) return 0;
    if (temp_1) *temp_1 = data.temp_1;
    if (temp_2) *temp_2 = data.temp_2;
    return 1;
}
