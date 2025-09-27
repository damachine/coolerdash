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
 * @brief LCD image upload implementation for CoolerDash.
 * @details Provides functions for uploading PNG images to the LCD display of CoolerControl devices via the REST API.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "upload.h"
#include "coolercontrol.h"
#include "config.h"
#include "session.h"

 // Session handle from coolercontrol.c
extern CoolerControlSession cc_session;

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

/**
 * @brief Add the image file part to the multipart form.
 * @details Uses curl_mime_filedata to attach the PNG file to the form.
 */
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
 * @brief Populate the multipart form with required fields.
 * @details Adds mode, brightness, orientation fields to the form.
 */
static int populate_upload_fields(curl_mime *form, const Config *config)
{
    char tmp[16];
    if (!form || !config)
        return 0;
    if (!add_mime_field(form, "mode", "image"))
        return 0;
    if (snprintf(tmp, sizeof(tmp), "%d", config->lcd_brightness) < 0)
        return 0;
    if (!add_mime_field(form, "brightness", tmp))
        return 0;
    if (snprintf(tmp, sizeof(tmp), "%d", config->lcd_orientation) < 0)
        return 0;
    if (!add_mime_field(form, "orientation", tmp))
        return 0;
    return 1;
}

/**
 * @brief Append the image file to the multipart form.
 * @details Adds the image file part to the CURL MIME form.
 */
static int append_image_file_to_form(curl_mime *form, const char *image_path)
{
    if (!form || !image_path)
        return 0;
    if (add_mime_file(form, "images[]", image_path))
        return 1;
    struct stat st;
    if (stat(image_path, &st) == 0)
        log_message(LOG_ERROR, "Failed to set image file data");
    return 0;
}

/**
 * @brief Create and populate the CURL MIME form for LCD image upload.
 * @details Initializes the form and adds all required fields and the image file.
 */
static curl_mime *create_upload_mime(const Config *config, const char *image_path)
{
    if (!cc_session.curl_handle || !config || !image_path)
        return NULL;
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
 * @brief Build the upload URL for the specified device UID.
 * @details Constructs the full URL for uploading images to the device's LCD endpoint.
 */
static int build_upload_url(char *buf, size_t bufsize, const Config *config, const char *device_uid)
{
    if (!buf || !config || !device_uid)
        return 0;
    int sret = snprintf(buf, bufsize, "%s/devices/%s/settings/lcd/lcd/images",
                        config->daemon_address, device_uid);
    if (sret < 0 || (size_t)sret >= bufsize)
        return 0;
    return 1;
}

/**
 * @brief Setup CURL options for LCD image upload.
 * @details Configures URL, headers, response handling and SSL options.
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
 * @brief Validate parameters for prepare_lcd_upload function.
 * @details Ensures all required parameters are non-null and session is initialized.
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
 * @brief Prepare CURL options and multipart form for LCD image upload.
 * @details Sets up the CURL handle with URL, headers, response buffer and MIME form.
 */
int prepare_lcd_upload(const Config *config, const char *image_path, const char *device_uid,
                       curl_mime **out_form, struct http_response *out_response,
                       struct curl_slist **out_headers)
{
    if (!validate_prepare_lcd_upload_params(config, image_path, device_uid, out_form, out_response, out_headers))
        return 0;
    char upload_url[CC_URL_SIZE];
    if (!build_upload_url(upload_url, sizeof(upload_url), config, device_uid))
    {
        log_message(LOG_ERROR, "Upload URL too long or invalid");
        return 0;
    }
    *out_form = create_upload_mime(config, image_path);
    if (!*out_form)
    {
        log_message(LOG_ERROR, "Failed to create upload form");
        return 0;
    }
    if (!setup_curl_upload_options(config, out_response, out_headers))
    {
        log_message(LOG_ERROR, "Failed to setup CURL options for upload");
        curl_mime_free(*out_form);
        *out_form = NULL;
        return 0;
    }
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, *out_form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    return 1;
}

/**
 * @brief Perform the LCD image upload and finalize the CURL request.
 * @details Executes the CURL request and cleans up temporary data.
 */
int perform_lcd_upload_and_finalize(struct http_response *response)
{
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
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);
    return success;
}

/**
 * @brief Send image to LCD of Liquidctl device.
 * @details Uploads a PNG file to the LCD display of the specified Liquidctl device using the CoolerControl REST API.
 */
int send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid)
{
    if (!cc_session.curl_handle || !image_path || !device_uid || !is_session_initialized())
    {
        log_message(LOG_ERROR, "Invalid parameters or session not initialized");
        return 0;
    }
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
    int success = perform_lcd_upload_and_finalize(&response);
    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    if (form)
        curl_mime_free(form);
    return success;
}
