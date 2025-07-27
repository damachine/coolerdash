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

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"
#include "../include/cpu_monitor.h"
#include "../include/gpu_monitor.h"
#include "../include/display.h"

// Include necessary headers
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/**
 * @brief Global variables for daemon management.
 * @details Used for controlling the main daemon loop and shutdown image logic.
 * @example
 *     // Not intended for direct use; managed by main and signal handler.
 */
static volatile sig_atomic_t running = 1; // flag whether daemon is running
static volatile sig_atomic_t shutdown_sent = 0; // flag whether shutdown image was already sent

/**
 * @brief Global pointer to config for signal handler access.
 * @details Allows the signal handler to access configuration data for cleanup and shutdown image.
 * @example
 *     // Not intended for direct use; set in main().
 */
static const Config *g_config_ptr = NULL;

/**
 * @brief Signal handler for clean daemon termination with shutdown image.
 * @details Sends a shutdown image to the LCD (if not already sent), removes the PID file, and sets the running flag to 0 for clean termination.
 * @example
 *     signal(SIGTERM, cleanup_and_exit);
 */
static void cleanup_and_exit(int sig) {
    (void)sig; // parameter is not used
    // Send shutdown image
    if (!shutdown_sent && is_session_initialized() && g_config_ptr) {
        const char* shutdown_image = g_config_ptr->paths_image_shutdown;
        const char* device_uid = get_cached_device_uid();
        fflush(stdout);
        if (device_uid[0]) {
            // Send shutdown image to LCD
            send_image_to_lcd(g_config_ptr, shutdown_image, device_uid);
            send_image_to_lcd(g_config_ptr, shutdown_image, device_uid); // send twice for reliability
            shutdown_sent = 1; // set flag so it's only sent once
        }
        fflush(stdout);
    }
    unlink(g_config_ptr->paths_pid); // remove PID file
    running = 0; // set flag to terminate daemon
}

/**
 * @brief Check if another instance of CoolerDash is running (systemd or process).
 * @details Uses systemctl and pgrep to detect running service or process. Returns -1 if another instance is active, 0 otherwise.
 * @example
 *     check_existing_instance_and_handle(config.pid_file, is_service_start);
 */
static int check_existing_instance_and_handle(const char *pid_file, int is_service_start) {
    (void)pid_file;
    // Skip service check if started by systemd
    if (!is_service_start) {
        int status = system("systemctl is-active --quiet coolerdash.service");
        if (status == 0) {
            printf("CoolerDash: Error - systemd service is already running\n");
            printf("Stop the service first: sudo systemctl stop coolerdash.service\n");
            return -1;
        }
    }
    // Check for running process using pgrep
    FILE *fp = popen("pgrep -x coolerdash", "r");
    int found_pid = 0;
    if (fp) {
        int pid;
        while (fscanf(fp, "%d", &pid) == 1) {
            if (!is_service_start || pid != getpid()) {
                found_pid = pid;
                break;
            }
        }
        pclose(fp);
    }
    if (found_pid > 0) {
        printf("CoolerDash: Error - another coolerdash process is already running (PID %d)\n", found_pid);
        printf("Stop it first: kill %d\n", found_pid);
        return -1;
    }
    return 0;
}

/**
 * @brief Write current PID to PID file.
 * @details Writes the current process ID to the specified PID file for single-instance enforcement.
 * @example
 *     write_pid_file("/var/run/coolerdash.pid");
 */
static void write_pid_file(const char *pid_file) {
    FILE *f = fopen(pid_file, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
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
           config->display_refresh_interval_sec, config->display_refresh_interval_nsec / 100000000);
    printf("Daemon now running...\n\n");
    fflush(stdout);
    while (running) { // Main daemon loop
        draw_combined_image(config); // Draw combined image
        struct timespec ts = {config->display_refresh_interval_sec, config->display_refresh_interval_nsec}; // Wait time for update
        nanosleep(&ts, NULL); // Wait for specified time
    }
    // Silent termination without output
    return 0;
}

/**
 * @brief Detect if we were started by systemd.
 * @details Checks if the parent process is PID 1 (systemd/init). Returns 1 if started by systemd, 0 otherwise.
 * @example
 *     int is_service = is_started_by_systemd();
 */
static int is_started_by_systemd(void) {
    // Simple and reliable method: Check if our parent process is PID 1 (init/systemd)
    return (getppid() == 1);
}

/**
 * @brief Show help and explain program usage.
 * @details Prints usage information and help text to stdout. Uses printf().
 * @example
 *     show_help(argv[0], &config);
 */
static void show_help(const char *program_name, const Config *config) {
    (void)config; // Mark parameter as unused to avoid compiler warning
    printf("\033[1mCoolerDash - LCD dashboard for CoolerControl\033[0m\n");
    printf("This program is a daemon that displays CPU and GPU temperatures on an LCD screen.\n");
    printf("For help, use: man coolerdash\n");
    printf("For help, refer to the documentation at README.md\n\n");
    printf("Usage: %s\n", program_name);
    printf("To start service: sudo systemctl start coolerdash\n");
    printf("To start manually: %s [config_path]\n", program_name);
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

    // Load configuration from INI file
    const char *default_config_path = "/opt/coolerdash/config.ini";
    const char *fallback_config_path = "/etc/coolerdash/config.ini";
    const char *config_path = NULL;

    FILE *f = fopen(default_config_path, "r");
    if (f) {
        fclose(f);
        config_path = default_config_path;
    } else {
        printf("Config file not found at %s. Using fallback: %s\n", default_config_path, fallback_config_path);
        config_path = fallback_config_path;
    }

    Config config;
    if (load_config_ini(&config, config_path) != 0) {
        fprintf(stderr, "Error: Could not load config file '%s'\n", config_path);
        return 1;
    }
    
    // Check if we were started by systemd
    int is_service_start = is_started_by_systemd();

    // Single-Instance Enforcement: Check and handle existing instances
    if (check_existing_instance_and_handle(config.paths_pid, is_service_start) < 0) {
        // Error: Service already running and we are manual start
        return 1;
    }

    // Write new PID file
    write_pid_file(config.paths_pid);
    g_config_ptr = &config; // Set global pointer for signal handler

    // Register signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = cleanup_and_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Initialize CoolerControl session
    if (init_coolercontrol_session(&config)) { // Check return value
        printf("✓ CoolerControl session initialized\n");

        // Get and display LCD device UID only if detected and changed
        static char last_device_uid[128] = {0};
        char device_uid[128] = {0};
        get_device_uid(&config, device_uid, sizeof(device_uid));
        if (strcmp(last_device_uid, device_uid) != 0) {
            printf("✓ Detected LCD device UID: %.20s...\n", device_uid);
            strncpy(last_device_uid, device_uid, sizeof(last_device_uid));
        }

        // Get and display full LCD device name only if detected
        char device_name[128] = {0};
        if (get_device_name(&config, device_name, sizeof(device_name))) {
            printf("✓ Connected to LCD device: %s\n", device_name);
        } else {
            printf("✓ Connected to LCD device: Unknow\n");
        }
        fflush(stdout);
    } else {
        fprintf(stderr, "⚠ Error: CoolerControl session could not be initialized\n");
        fprintf(stderr, "Please check:\n");
        fprintf(stderr, "  - Is coolercontrold running? (systemctl status coolercontrold)\n");
        fprintf(stderr, "  - Is the daemon running on localhost:11987?\n");
        fprintf(stderr, "  - Is the password correct? (see config.h)\n");
        fflush(stderr);
        return 1;
    }
    if (!init_cached_device_uid(&config)) {
        fprintf(stderr, "⚠ CoolerDash: Failed to detect LCD device UID\n");
        return 1;
    }

    // Create image directory
    mkdir(config.paths_images, 0755); // Create directory for images if not present
    printf("✓ Image directory: %s\n", config.paths_image_coolerdash);
    fflush(stdout);

    // Initialize CPU sensors
    init_cpu_sensor_path(&config); // Set path to CPU sensors
    printf("✓ CPU monitor initialized\n");
    fflush(stdout);

    // Initialize GPU monitor (if GPU available)
    if (init_gpu_monitor(&config)) { // Check return value
        printf("✓ GPU monitor initialized\n");
    } else {
        printf("⚠ GPU monitor not available (no NVIDIA GPU?)\n");
    }
    fflush(stdout);

    // Start daemon
    int result = run_daemon(&config);
    
    // Cleanup - send shutdown image if not sent yet (only on normal termination)
    if (!shutdown_sent && is_session_initialized()) {
        const char* shutdown_image = g_config_ptr->paths_image_shutdown;
        const char* device_uid = get_cached_device_uid();
        fflush(stdout);
        if (device_uid[0]) {
            send_image_to_lcd(&config, shutdown_image, device_uid);
            send_image_to_lcd(&config, shutdown_image, device_uid);
            shutdown_sent = 1; // set flag so it's only sent once
        }
        fflush(stdout);
    }
    cleanup_coolercontrol_session(); // Terminate CoolerControl session
    cleanup_and_exit(0); // Remove PID file and terminate daemon
    return result;
}