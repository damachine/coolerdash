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
#ifndef SESSION_H
#define SESSION_H

// Include project headers
#include "config.h"

/**
 * @brief Initialize the CoolerControl session.
 * @details Sets up CURL, performs authentication, and prepares the session for API communication.
 */
int init_coolercontrol_session(const Config *config);
int is_session_initialized(void);
void cleanup_coolercontrol_session(void);

#endif // SESSION_H
