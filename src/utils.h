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
 * @brief Utility functions for CoolerDash.
 * @details Provides common utility functions such as secure string copy, memory allocation, HTTP response handling, and JSON parsing helpers.
 */
#ifndef UTILS_H
#define UTILS_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <jansson.h>
#include <curl/curl.h>
// cppcheck-suppress-end missingIncludeSystem

int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src);
void *cc_secure_malloc(size_t size);
size_t write_callback(char *contents, size_t size, size_t nmemb, void *userp);
const char *extract_device_type_from_json(json_t *dev);

#endif // UTILS_H
