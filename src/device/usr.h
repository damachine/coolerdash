/**
 * -----------------------------------------------------------------------------
 * Created by: damachine (damachine3 at proton dot me)
 * Website: https://github.com/damachine/coolerdash
 * -----------------------------------------------------------------------------
 */

/**
 * @brief User configuration file parsing.
 * @details Provides INI file parsing for user-defined configuration values.
 *          User values always take precedence over system defaults.
 */

#ifndef USR_H
#define USR_H

// Include project headers
#include "sys.h"

/**
 * @brief Load user configuration from INI file.
 * @details Parses INI configuration file and populates Config structure.
 *          Only sets values that are explicitly defined in the INI file.
 *          Missing values remain unchanged (allowing system defaults to apply).
 *
 *          Call order: 1) init_system_defaults() 2) load_user_config() 3)
 * apply_system_defaults()
 *
 * @param path Path to INI configuration file
 * @param config Pointer to Config structure to populate
 * @return 0 on success, -1 on error
 */
int load_user_config(const char *path, Config *config);

#endif // USR_H
