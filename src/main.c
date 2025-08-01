/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Main entry point for CoolerDash daemon.
 * @details Implements the main daemon logic, including initialization, single-instance enforcement, signal handling, and main loop.
 * @example
 *     See function documentation for usage examples.
 */

// Function prototypes
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

// Include necessary headers
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"
#include "../include/display.h"
#include "../include/monitor.h"

/**
 * @brief Global variables for daemon management.
 * @details Used for controlling the main daemon loop and shutdown image logic.
 * @example
 *     static volatile sig_atomic_t running = 1; // flag whether daemon is running
 *     static volatile sig_atomic_t shutdown_sent = 0; // flag whether shutdown image was already sent
 */
static volatile sig_atomic_t running = 1; // flag whether daemon is running
static volatile sig_atomic_t shutdown_sent = 0; // flag whether shutdown image was already sent

/**
 * @brief Global pointer to configuration.
 * @details Points to the current configuration used by the daemon. Initialized in main().
 * @example
 *     g_config_ptr = &config;
 */
const Config *g_config_ptr = NULL;

/**
 * @brief Check if another instance of CoolerDash is running (systemd or process).
 * @details Uses systemctl and pgrep to detect running service or process. Returns -1 if another instance is active, 0 otherwise.
 * @example
 *     check_existing_instance_and_handle(config.pid_file, is_service_start);
 */
static int check_existing_instance_and_handle(const char *pid_file, int is_service_start) {
    (void)is_service_start;
    
    FILE *fp = fopen(pid_file, "r");
    if (fp) {
        int existing_pid;
        if (fscanf(fp, "%d", &existing_pid) == 1) {
            fclose(fp);
            // Check if process exists using kill(pid, 0)
            if (kill(existing_pid, 0) == 0) {
                printf("CoolerDash: Error - another instance is running (PID %d)\n", existing_pid);
                return -1;
            } else if (errno == EPERM) {
                printf("CoolerDash: Error - another instance may be running (PID %d)\n", existing_pid);
                return -1;
            }
            // Process doesn't exist, remove stale PID file
            unlink(pid_file);
        } else {
            fclose(fp);
        }
    }
    
    return 0;
}

/**
 * @brief Write current PID to PID file.
 * @details Writes the current process ID to the specified PID file for single-instance enforcement.
 * @example
 *     write_pid_file("/var/run/coolerdash.pid");
 */
static int write_pid_file(const char *pid_file)
{
    FILE *f = fopen(pid_file, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not open PID file '%s' for writing\n", pid_file);
        return -1;
    }
    
    if (fprintf(f, "%d\n", getpid()) < 0) {
        fprintf(stderr, "Error: Could not write PID to file '%s'\n", pid_file);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

/**
 * @brief Remove PID file if it exists.
 * @details Attempts to remove the PID file specified. If the file does not exist, it ignores the error.
 * @example
 *    remove_pid_file("/var/run/coolerdash.pid");
 */
static void remove_pid_file(const char *pid_file)
{
    // Skip if pid_file is NULL or empty
    if (!pid_file || !pid_file[0]) {
        return;
    }
    
    // Attempt to remove the file
    if (unlink(pid_file) == -1 && errno != ENOENT) {
        fprintf(stderr, "Warning: Could not remove PID file '%s': %s\n", 
                pid_file, strerror(errno));
    }
}

/**
 * @brief Main daemon loop.
 * @details Runs the main loop of the daemon, periodically updating the display with sensor data until termination is requested. Uses draw_combined_image(), nanosleep(). Checks the running flag for termination. Returns 0 on normal exit.
 * @example
 *     int result = run_daemon(&config);
 */
static int run_daemon(const Config *config) {
    printf("CoolerDash daemon started\n");
    printf("Sensor data updated every %d.%d seconds\n", 
           config->display_refresh_interval_sec, 
           config->display_refresh_interval_nsec / 100000000);
    printf("Daemon now running...\n\n");
    fflush(stdout);
    
    const struct timespec interval = {
        .tv_sec = config->display_refresh_interval_sec,
        .tv_nsec = config->display_refresh_interval_nsec
    };
    
    struct timespec next_time;
    clock_gettime(CLOCK_MONOTONIC, &next_time);

    while (running) {
        // Calculate next execution time
        next_time.tv_sec += interval.tv_sec;
        next_time.tv_nsec += interval.tv_nsec;
        if (next_time.tv_nsec >= 1000000000) {
            next_time.tv_sec++;
            next_time.tv_nsec -= 1000000000;
        }
        
        draw_combined_image(config);
        
        // Sleep until absolute time
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
    }
    
    return 0;
}

/**
 * @brief Detect if we were started by systemd.
 * @details Checks if the parent process is PID 1 (systemd/init). Returns 1 if started by systemd, 0 otherwise.
 * @example
 *     int is_service = is_started_by_systemd();
 */
static int is_started_by_systemd(void) {
    return getppid() == 1;
}

/**
 * @brief Show help and explain program usage.
 * @details Prints usage information and help text to stdout. Uses printf().
 * @example
 *     show_help(argv[0], &config);
 */
static void show_help(const char *program_name, const Config *config) {
    (void)config; // Mark parameter as unused to avoid compiler warning
    
    printf("CoolerDash - LCD dashboard for CoolerControl\n"
           "This program is a daemon that displays CPU and GPU temperatures on an LCD screen.\n"
           "For help, use: man coolerdash\n"
           "For help, refer to the documentation at README.md\n\n"
           "Usage: %s\n"
           "To start service: sudo systemctl start coolerdash\n"
           "To start manually: %s [config_path]\n",
           program_name, program_name);
}

static void send_shutdown_image_if_needed(void) {
    if (!shutdown_sent && is_session_initialized() && g_config_ptr) {
        cc_sensor_data_t shutdown_data = {0};
        if (monitor_get_sensor_data(g_config_ptr, &shutdown_data)) {
            if (shutdown_data.device_uid[0] != '\0') {
                send_image_to_lcd(g_config_ptr, g_config_ptr->paths_image_shutdown, shutdown_data.device_uid);
                send_image_to_lcd(g_config_ptr, g_config_ptr->paths_image_shutdown, shutdown_data.device_uid); // Send twice to ensure upload and no artfact is displayed
                shutdown_sent = 1;
            }
        }
    }
}

/**
 * @brief Signal handler for clean shutdown (sends shutdown image).
 * @details Sends the shutdown image to the LCD if not already sent and UID is available, then sets running=0.
 * @example
 *     handle_shutdown_signal(signum);
 */
static void handle_shutdown_signal(int signum)
{
    (void)signum;  // Suppress unused parameter warning
    send_shutdown_image_if_needed();  // Send shutdown image immediately
    running = 0;   // Signal graceful shutdown
}

/**
 * @brief Main entry point for CoolerDash.
 * @details Loads configuration, ensures config file exists, initializes modules, and starts the main daemon loop.
 * @example
 *     coolerdash [config_path]
 */
int main(int argc, char **argv)
{
    // Check for help argument
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help(argv[0], NULL);
        return 0;
    }

    // Load configuration
    const char *config_path = "/opt/coolerdash/config.ini";
    Config config = {0};
    if (load_config_ini(&config, config_path) != 0) {
        fprintf(stderr, "Error: Could not load config file '%s'\n", config_path);
        return 1;
    }

    // Check for existing instances and create PID file
    int is_service_start = is_started_by_systemd();
    if (check_existing_instance_and_handle(config.paths_pid, is_service_start) < 0 ||
        write_pid_file(config.paths_pid) != 0) {
        return 1;
    }
    g_config_ptr = &config;

    // Register signal handlers for clean shutdown
    struct sigaction sa = {
        .sa_handler = handle_shutdown_signal,
        .sa_flags = 0
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Initialize CoolerControl session
    if (!init_coolercontrol_session(&config)) {
        fprintf(stderr, "Error: CoolerControl session could not be initialized\n"
                       "Please check:\n"
                       "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
                       "  - Is the daemon running on localhost:11987?\n"
                       "  - Is the password correct? (see config.h)\n");
        fflush(stderr);
        remove_pid_file(config.paths_pid);
        return 1;
    }

    printf("Coolercontrol sensor API initialized\n");
    fflush(stdout);

    // Retrieve and display device information
    cc_sensor_data_t cc_data = {0};
    if (monitor_get_sensor_data(&config, &cc_data)) {
        const char *uid_msg = (cc_data.device_uid[0] != '\0') 
            ? cc_data.device_uid 
            : "Unknown device UID detected";
        fprintf(stderr, "CoolerDash connected device UID: %s\n", uid_msg);
    } else {
        fprintf(stderr, "CoolerDash could not retrieve device UID and name.\n");
    }

    printf("CoolerDash read config file:\n");
    fflush(stdout);

    // Run daemon and cleanup
    int result = run_daemon(&config);
    send_shutdown_image_if_needed(); // Ensure shutdown image is sent on normal exit
    remove_pid_file(config.paths_pid);
    running = 0;
    return result;
}