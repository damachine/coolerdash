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
#ifndef UPLOAD_H
#define UPLOAD_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "config.h"
#include "coolercontrol.h"

int prepare_lcd_upload(const Config *config, const char *image_path, const char *device_uid,
                       curl_mime **out_form, struct http_response *out_response,
                       struct curl_slist **out_headers);
int perform_lcd_upload_and_finalize(struct http_response *response);

/**
 * @brief Sendet ein Bild an das LCD-Display.
 * @details Upload einer PNG-Datei an das LCD des Liquidctl-Geräts.
 */
int send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid);

#endif // UPLOAD_H
