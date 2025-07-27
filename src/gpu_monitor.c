/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief GPU temperature monitoring implementation for CoolerDash.
 * @details Implements functions for reading GPU temperature from system sensors and handling GPU availability.
 * @example
 *     See function documentation for usage examples.
 */

// Include project headers
#include "../include/gpu_monitor.h"
#include "../include/config.h"

// Include necessary headers
#include <stdio.h>

/**
 * @brief Global variable for GPU availability.
 * @details Indicates if an NVIDIA GPU is available: -1 = unknown, 0 = not available, 1 = available.
 * @example
 *     // Not intended for direct use; managed by init_gpu_monitor().
 */
static int gpu_available = -1;  // -1 = unknown, 0 = not available, 1 = available

/**
 * @brief Checks GPU availability and initializes GPU monitoring using configuration.
 * @details Checks if an NVIDIA GPU is available and initializes the monitoring backend. Uses config values for cache interval etc. Internally uses popen() to check for nvidia-smi. Always checks and frees resources with pclose().
 * @example
 *     if (init_gpu_monitor(&config)) {
 *         // GPU available
 *     }
 */
int init_gpu_monitor(const Config *config) {
    (void)config; // suppress unused parameter warning (C99)
    
    if (gpu_available != -1) {
        return gpu_available;  // Already checked
    }
    
    FILE *fp = popen("nvidia-smi -L 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp) != NULL) {
            gpu_available = 1;  // GPU found
        } else {
            gpu_available = 0;  // No GPU found
        }
        pclose(fp);
    } else {
        gpu_available = 0;  // nvidia-smi not available
    }
    
    return gpu_available;
}

/**
 * @brief Reads only GPU temperature (optimized for mode "def").
 * @details Reads the current temperature from the GPU sensor, with caching for performance. Uses popen() and fscanf() to read the value. Always checks and frees resources with pclose(). Returns 0.0f if no GPU is available or on error.
 * @example
 *     float temp = read_gpu_temp(&config);
 */
float read_gpu_temp(const Config *config) {
    if (!init_gpu_monitor(config)) return 0.0f;  // GPU not available
    
    float temp = 0.0f;
    FILE *fp = popen("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
    if (fp) {
        if (fscanf(fp, "%f", &temp) != 1) {
            temp = 0.0f;
        }
        pclose(fp);
    }
    return temp;
}

/**
 * @brief Reads GPU temperature only (usage and memory usage always 0).
 * @details Fills a gpu_data_t structure with temperature value, usage and memory usage set to 0. Returns 1 on success, 0 on failure. Always check the return value and ensure the pointer is valid.
 * @example
 *     gpu_data_t data;
 *     if (get_gpu_data_full(&config, &data)) {
 *         // use data
 *     }
 */
int get_gpu_data_full(const Config *config, gpu_data_t *data) {
    if (!data) return 0;
    data->temperature = read_gpu_temp(config);
    return 1;
}
