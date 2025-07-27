/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CPU temperature monitoring implementation for CoolerDash.
 * @details Implements functions for reading CPU temperature from system sensors.
 * @example
 *     See function documentation for usage examples.
 */

// Include project headers
#include "../include/cpu_monitor.h"
#include "../include/config.h"

// Include necessary headers
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

/**
 * @brief Cached path to CPU temperature sensor file.
 * @details Set by init_cpu_sensor_path() and used by read_cpu_temp().
 * @example
 *     // Not intended for direct use; managed by module functions.
 */
char cpu_temp_path[512] = {0};

/**
 * @brief Initialize hwmon sensor path for CPU temperature at startup (once).
 * @details Detects and sets the path to the CPU temperature sensor file by scanning available hwmon entries. Uses the hwmon_path from the configuration struct. Internally uses opendir(), readdir(), fopen(), fclose(), closedir(). Always checks and frees resources. Returns nothing; path is cached in cpu_temp_path.
 * @example
 *     init_cpu_sensor_path(&config);
 */
void init_cpu_sensor_path(const Config *config) {
    DIR *dir = opendir(config->paths_hwmon);
    if (!dir) return;
    
    struct dirent *entry;
    char label_path[512], label[64];
    
    // Scan through hwmon directory entries
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Skip hidden files/directories
        
        // Check each possible temp label for the current hwmon entry
        for (int i = 1; i <= 9; ++i) {
            snprintf(label_path, sizeof(label_path), "%s/%s/temp%d_label", config->paths_hwmon, entry->d_name, i);
            FILE *flabel = fopen(label_path, "r");
            if (!flabel) continue;
            
            // Read the label and check if it matches "Package id 0"
            if (fgets(label, sizeof(label), flabel)) {
                // Cache CPU temperature path for later use
                if (strstr(label, "Package id 0") && strlen(cpu_temp_path) == 0) {
                    snprintf(cpu_temp_path, sizeof(cpu_temp_path), "%s/%s/temp%d_input", config->paths_hwmon, entry->d_name, i);
                    fclose(flabel);
                    closedir(dir);
                    return;
                }
            }
            fclose(flabel);
        }
    }
    closedir(dir);
}

/**
 * @brief Read CPU temperature from cached hwmon path.
 * @details Reads the temperature from the CPU sensor file set by init_cpu_sensor_path(). Uses fopen(), fscanf(), fclose(). Always checks and frees resources. Returns 0.0f on error or if no path is cached.
 * @example
 *     float temp = read_cpu_temp();
 */
float read_cpu_temp(void) {
    if (strlen(cpu_temp_path) == 0) return 0.0f;
    
    FILE *finput = fopen(cpu_temp_path, "r");
    if (!finput) return 0.0f;
    
    int t = 0;
    float temp = 0.0f;
    // Read the integer temperature value, convert to float
    if (fscanf(finput, "%d", &t) == 1) {
        temp = t > 200 ? t / 1000.0f : (float)t;
    }
    fclose(finput);
    return temp;
}
