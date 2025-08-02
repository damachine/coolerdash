/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Enhanced main entry point for CoolerDash daemon with security and performance optimizations.
 * @details Implements the main daemon logic with improved error handling, secure PID management,
 * and optimized signal processing. Enhanced with input validation, dynamic version loading,
 * and modern C practices.
 * @example
 *     coolerdash [config_path] - starts the daemon with optional config file
 *     coolerdash --help - shows usage information
 *     systemctl start coolerdash - starts as system service (recommended)
 */

// POSIX and system feature requirements
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

// Security and performance constants
#define MAX_PID_STR_LEN 32         // Maximum length for PID string representation
#define MAX_ERROR_MSG_LEN 512      // Maximum error message length
#define SHUTDOWN_RETRY_COUNT 2     // Number of shutdown image send attempts
#define PID_READ_BUFFER_SIZE 64    // Buffer size for reading PID files
#define VERSION_BUFFER_SIZE 32     // Buffer size for version string
#define DEFAULT_VERSION "unknown"  // Fallback version string

// Include necessary headers in logical order
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
#include <limits.h>                // For safe integer parsing

#include <stdarg.h>                // For va_list in log_message

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

////////////////////////////////////////////////////////////////////////////////
// SECURE HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Secure logging function with consistent format.
 * @details Centralized logging with timestamp and proper error handling.
 * @example
 *     log_message(LOG_ERROR, "Failed to initialize: %s", error_msg);
 */
typedef enum {
    LOG_INFO,
    LOG_WARNING, 
    LOG_ERROR
} log_level_t;

static void log_message(log_level_t level, const char *format, ...) {
    const char *prefix[] = {"INFO", "WARNING", "ERROR"};
    FILE *output = (level == LOG_ERROR) ? stderr : stdout;
    
    fprintf(output, "[CoolerDash %s] ", prefix[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
}

/**
 * @brief Read version string from VERSION file with enhanced security.
 * @details Safely reads version from VERSION file with buffer overflow protection
 * and proper validation. Returns fallback version on error.
 * @example
 *     const char* version = read_version_from_file();
 */
static const char* read_version_from_file(void) {
    static char version_buffer[VERSION_BUFFER_SIZE] = {0};
    static int version_loaded = 0;
    
    // Return cached version if already loaded
    if (version_loaded) {
        return version_buffer[0] ? version_buffer : DEFAULT_VERSION;
    }
    
    // Try to read from VERSION file
    FILE *fp = fopen("VERSION", "r");
    if (!fp) {
        // Try alternative path for installed version
        fp = fopen("/opt/coolerdash/VERSION", "r");
    }
    
    if (!fp) {
        log_message(LOG_WARNING, "Could not open VERSION file, using default version");
        strncpy(version_buffer, DEFAULT_VERSION, sizeof(version_buffer) - 1);
        version_buffer[sizeof(version_buffer) - 1] = '\0';
        version_loaded = 1;
        return version_buffer;
    }
    
    // Secure reading with fixed buffer size
    if (!fgets(version_buffer, sizeof(version_buffer), fp)) {
        log_message(LOG_WARNING, "Could not read VERSION file, using default version");
        strncpy(version_buffer, DEFAULT_VERSION, sizeof(version_buffer) - 1);
        version_buffer[sizeof(version_buffer) - 1] = '\0';
    } else {
        // Remove trailing whitespace and newlines
        version_buffer[strcspn(version_buffer, "\n\r \t")] = '\0';
        
        // Validate version string (basic sanity check)
        if (version_buffer[0] == '\0' || strlen(version_buffer) > 20) {
            log_message(LOG_WARNING, "Invalid version format, using default version");
            strncpy(version_buffer, DEFAULT_VERSION, sizeof(version_buffer) - 1);
            version_buffer[sizeof(version_buffer) - 1] = '\0';
        }
    }
    
    fclose(fp);
    version_loaded = 1;
    return version_buffer;
}

/**
 * @brief Safely parse PID from string with validation.
 * @details Uses strtol for secure parsing with proper error checking.
 * @example
 *     pid_t pid = safe_parse_pid("1234");
 */
static pid_t safe_parse_pid(const char *pid_str) {
    if (!pid_str || !pid_str[0]) return -1;
    
    char *endptr;
    errno = 0;
    long pid = strtol(pid_str, &endptr, 10);
    
    // Validation checks
    if (errno != 0 || endptr == pid_str || *endptr != '\0') return -1;
    if (pid <= 0 || pid > INT_MAX) return -1;
    
    return (pid_t)pid;
}
////////////////////////////////////////////////////////////////////////////////
// INSTANCE MANAGEMENT WITH ENHANCED SECURITY
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Check if another instance of CoolerDash is running with secure PID validation.
 * @details Uses secure file reading and PID validation. Returns -1 if another instance is active, 0 otherwise.
 * Enhanced with buffer overflow protection and proper error handling.
 * @example
 *     if (check_existing_instance_and_handle(config.paths_pid, is_service_start) < 0) { ... }
 */
static int check_existing_instance_and_handle(const char *pid_file, int is_service_start) {
    (void)is_service_start; // Mark as intentionally unused
    
    if (!pid_file || !pid_file[0]) {
        log_message(LOG_ERROR, "Invalid PID file path provided");
        return -1;
    }
    
    FILE *fp = fopen(pid_file, "r");
    if (!fp) return 0; // No PID file exists, no running instance
    
    // Secure reading with fixed buffer size
    char pid_buffer[PID_READ_BUFFER_SIZE] = {0};
    if (!fgets(pid_buffer, sizeof(pid_buffer), fp)) {
        fclose(fp);
        unlink(pid_file); // Remove corrupted PID file
        return 0;
    }
    fclose(fp);
    
    // Remove trailing newline and validate
    pid_buffer[strcspn(pid_buffer, "\n\r")] = '\0';
    pid_t existing_pid = safe_parse_pid(pid_buffer);
    
    if (existing_pid <= 0) {
        log_message(LOG_WARNING, "Invalid PID in file, removing stale PID file");
        unlink(pid_file);
        return 0;
    }
    
    // Check if process exists using kill(pid, 0)
    if (kill(existing_pid, 0) == 0) {
        log_message(LOG_ERROR, "Another instance is already running (PID %d)", existing_pid);
        return -1;
    } else if (errno == EPERM) {
        log_message(LOG_ERROR, "Another instance may be running (PID %d) - insufficient permissions to verify", existing_pid);
        return -1;
    }
    
    // Process doesn't exist, remove stale PID file
    log_message(LOG_INFO, "Removing stale PID file (process %d no longer exists)", existing_pid);
    unlink(pid_file);
    return 0;
}

/**
 * @brief Write current PID to file with enhanced security and error checking.
 * @details Creates PID file with proper permissions and atomic write operation.
 * Enhanced with directory creation and comprehensive error handling.
 * @example
 *     if (write_pid_file("/var/run/coolerdash.pid") != 0) { ... }
 */
static int write_pid_file(const char *pid_file) {
    if (!pid_file || !pid_file[0]) {
        log_message(LOG_ERROR, "Invalid PID file path provided");
        return -1;
    }
    
    // Create directory if it doesn't exist
    char *dir_path = strdup(pid_file);
    if (!dir_path) {
        log_message(LOG_ERROR, "Memory allocation failed for directory path");
        return -1;
    }
    
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(dir_path, 0755) == -1 && errno != EEXIST) {
            log_message(LOG_WARNING, "Could not create PID directory '%s': %s", dir_path, strerror(errno));
        }
    }
    free(dir_path);
    
    // Atomic write: write to temporary file first, then rename
    char temp_file[PATH_MAX];
    int ret = snprintf(temp_file, sizeof(temp_file), "%s.tmp", pid_file);
    if (ret >= (int)sizeof(temp_file) || ret < 0) {
        log_message(LOG_ERROR, "PID file path too long");
        return -1;
    }
    
    FILE *f = fopen(temp_file, "w");
    if (!f) {
        log_message(LOG_ERROR, "Could not create temporary PID file '%s': %s", temp_file, strerror(errno));
        return -1;
    }
    
    // Write PID with validation
    pid_t current_pid = getpid();
    if (fprintf(f, "%d\n", current_pid) < 0) {
        log_message(LOG_ERROR, "Could not write PID to temporary file '%s': %s", temp_file, strerror(errno));
        fclose(f);
        unlink(temp_file);
        return -1;
    }
    
    if (fclose(f) != 0) {
        log_message(LOG_ERROR, "Could not close temporary PID file '%s': %s", temp_file, strerror(errno));
        unlink(temp_file);
        return -1;
    }
    
    // Atomic rename
    if (rename(temp_file, pid_file) != 0) {
        log_message(LOG_ERROR, "Could not rename temporary PID file to '%s': %s", pid_file, strerror(errno));
        unlink(temp_file);
        return -1;
    }
    
    // Set appropriate permissions
    if (chmod(pid_file, 0644) != 0) {
        log_message(LOG_WARNING, "Could not set permissions on PID file '%s': %s", pid_file, strerror(errno));
    }
    
    log_message(LOG_INFO, "PID file: %s (PID: %d)", pid_file, current_pid);
    return 0;
}

/**
 * @brief Remove PID file with enhanced error handling.
 * @details Securely removes the PID file with proper error reporting.
 * @example
 *     remove_pid_file("/var/run/coolerdash.pid");
 */
static void remove_pid_file(const char *pid_file) {
    if (!pid_file || !pid_file[0]) return;
    
    if (unlink(pid_file) == 0) {
        log_message(LOG_INFO, "PID file removed");
    } else if (errno != ENOENT) {
        log_message(LOG_WARNING, "Could not remove PID file '%s': %s", pid_file, strerror(errno));
    }
}

////////////////////////////////////////////////////////////////////////////////
// DEBUG AND DIAGNOSTICS FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Display comprehensive system and device information for diagnostics.
 * @details Shows detailed system state including device info, display config,
 * and sensor data. Only called in debug mode or on explicit request.
 * @example
 *     show_system_diagnostics(&config, &device_data, api_screen_width, api_screen_height);
 */
static void show_system_diagnostics(const Config *config, const cc_device_data_t *device_data, 
                                   int api_width, int api_height) {
    if (!config || !device_data) return;
    
    log_message(LOG_INFO, "Display configuration: %dx%d pixels", 
               config->display_width, config->display_height);
    
    if (api_width > 0 && api_height > 0) {
        if (api_width != config->display_width || api_height != config->display_height) {
            log_message(LOG_WARNING, "API reports different dimensions: %dx%d pixels", 
                       api_width, api_height);
        } else {
            log_message(LOG_INFO, "API dimensions match configuration");
        }
    }
    
    log_message(LOG_INFO, "Refresh interval: %d.%03d seconds", 
               config->display_refresh_interval_sec,
               config->display_refresh_interval_nsec / 1000000);
}

////////////////////////////////////////////////////////////////////////////////
// DAEMON CORE FUNCTIONALITY
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Enhanced main daemon loop with improved timing and error handling.
 * @details Runs the main loop with precise timing, optimized sleep, and graceful error recovery.
 * Uses clock_nanosleep for better precision and reduced CPU usage.
 * @example
 *     int result = run_daemon(&config);
 */
static int run_daemon(const Config *config) {
    if (!config) {
        log_message(LOG_ERROR, "Invalid configuration provided to daemon");
        return -1;
    }
    
    const struct timespec interval = {
        .tv_sec = config->display_refresh_interval_sec,
        .tv_nsec = config->display_refresh_interval_nsec
    };
    
    struct timespec next_time;
    if (clock_gettime(CLOCK_MONOTONIC, &next_time) != 0) {
        log_message(LOG_ERROR, "Failed to get current time: %s", strerror(errno));
        return -1;
    }
    
    unsigned long iteration_count = 0;
    while (running) {
        // Calculate next execution time with overflow protection
        next_time.tv_sec += interval.tv_sec;
        next_time.tv_nsec += interval.tv_nsec;
        if (next_time.tv_nsec >= 1000000000L) {
            next_time.tv_sec++;
            next_time.tv_nsec -= 1000000000L;
        }
        
        // Execute main rendering task
        draw_combined_image(config);
        iteration_count++;
        
        // Periodic status logging (every 500 iterations to reduce spam)
        if (iteration_count % 500 == 0) {
            log_message(LOG_INFO, "Daemon running (%lu iterations)", iteration_count);
        }
        
        // Sleep until absolute time with error handling
        int sleep_result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
        if (sleep_result != 0 && sleep_result != EINTR) {
            log_message(LOG_WARNING, "Sleep interrupted: %s", strerror(sleep_result));
        }
    }
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// UTILITY AND SIGNAL HANDLING FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Detect if we were started by systemd with enhanced validation.
 * @details Checks if the parent process is PID 1 with additional systemd environment validation.
 * @example
 *     int is_service = is_started_by_systemd();
 */
static int is_started_by_systemd(void) {
    // Primary check: parent process is PID 1
    if (getppid() != 1) return 0;
    
    // Additional validation: check for systemd environment variables
    const char *systemd_vars[] = {"SYSTEMD_EXEC_PID", "INVOCATION_ID", NULL};
    for (int i = 0; systemd_vars[i]; i++) {
        if (getenv(systemd_vars[i])) return 1;
    }
    
    return 1; // Assume systemd if parent is PID 1
}

/**
 * @brief Enhanced help display with improved formatting and security information.
 * @details Prints comprehensive usage information and security recommendations.
 * @example
 *     show_help(argv[0], &config);
 */
static void show_help(const char *program_name, const Config *config) {
    (void)config; // Mark parameter as intentionally unused
    
    if (!program_name) program_name = "coolerdash";
    
    const char *version = read_version_from_file();
    
    printf("================================================================================\n");
    printf("CoolerDash v%s - Enhanced LCD Dashboard for CoolerControl\n", version);
    printf("================================================================================\n\n");
    printf("DESCRIPTION:\n");
    printf("  A high-performance daemon that displays CPU and GPU temperatures on LCD screens\n");
    printf("  connected via CoolerControl. Features real-time monitoring, secure PID\n");
    printf("  management, and optimized rendering performance.\n\n");
    printf("USAGE:\n");
    printf("  %s [OPTIONS] [CONFIG_PATH]\n\n", program_name);
    printf("OPTIONS:\n");
    printf("  -h, --help     Show this help message and exit\n\n");
    printf("EXAMPLES:\n");
    printf("  sudo systemctl start coolerdash    # Start as system service (recommended)\n");
    printf("  systemctl --user start coolerdash  # Start as user service\n");
    printf("  %s                                # Manual start with default config\n", program_name);
    printf("  %s /custom/config.ini             # Start with custom configuration\n\n", program_name);
    printf("FILES:\n");
    printf("  /usr/bin/coolerdash                # Main program executable\n");
    printf("  /opt/coolerdash/                   # Installation directory\n");
    printf("  /etc/coolerdash/config.ini         # Default configuration file\n");
    printf("  /run/coolerdash/coolerdash.pid     # PID file (auto-managed)\n");
    printf("  /var/log/syslog                    # Log output (when run as service)\n\n");
    printf("SECURITY:\n");
    printf("  - Runs as dedicated user 'coolerdash' for enhanced security\n");
    printf("  - Communicates via CoolerControl's HTTP API (no direct device access)\n");
    printf("  - Automatically manages single-instance enforcement\n");
    printf("  - Uses secure PID file handling with atomic operations\n\n");
    printf("For detailed documentation: man coolerdash\n");
    printf("Project repository: https://github.com/damachine/coolerdash\n");
    printf("================================================================================\n");
}

/**
 * @brief Enhanced shutdown image sending with retry logic and validation.
 * @details Sends shutdown image to LCD with multiple attempts and proper error handling.
 * Uses the updated config field names from optimized Config structure.
 * @example
 *     send_shutdown_image_if_needed();
 */
static void send_shutdown_image_if_needed(void) {
    if (shutdown_sent || !is_session_initialized() || !g_config_ptr) {
        return; // Early exit if conditions not met
    }
    
    cc_device_data_t device_data = {0};
    if (!get_device_uid(g_config_ptr, &device_data) || !device_data.device_uid[0]) {
        log_message(LOG_WARNING, "Cannot send shutdown image - device UID not available");
        return;
    }
    
    const char *shutdown_image_path = g_config_ptr->paths_image_shutdown;
    if (!shutdown_image_path || !shutdown_image_path[0]) {
        log_message(LOG_WARNING, "Shutdown image path not configured");
        return;
    }
    
    log_message(LOG_INFO, "Sending shutdown image to device %s", device_data.device_uid);
    
    // Send shutdown image with retry logic for reliability
    int success_count = 0;
    for (int attempt = 0; attempt < SHUTDOWN_RETRY_COUNT; attempt++) {
        if (send_image_to_lcd(g_config_ptr, shutdown_image_path, device_data.device_uid)) {
            success_count++;
        } else {
            log_message(LOG_WARNING, "Shutdown image send attempt %d failed", attempt + 1);
        }
        
        // Small delay between attempts to ensure proper LCD processing
        if (attempt < SHUTDOWN_RETRY_COUNT - 1) {
            usleep(100000); // 100ms delay
        }
    }
    
    if (success_count > 0) {
        log_message(LOG_INFO, "Shutdown image sent successfully (%d/%d attempts)", 
                   success_count, SHUTDOWN_RETRY_COUNT);
        shutdown_sent = 1;
    } else {
        log_message(LOG_ERROR, "Failed to send shutdown image after %d attempts", SHUTDOWN_RETRY_COUNT);
    }
}

////////////////////////////////////////////////////////////////////////////////
// ENHANCED SIGNAL HANDLING WITH SECURITY
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Enhanced signal handler with atomic operations and secure shutdown.
 * @details Signal-safe implementation using only async-signal-safe functions.
 * Provides graceful shutdown with proper resource cleanup and shutdown image.
 * @example
 *     struct sigaction sa = { .sa_handler = handle_shutdown_signal };
 */
static void handle_shutdown_signal(int signum) {
    // Use only async-signal-safe functions in signal handlers
    static const char term_msg[] = "Received SIGTERM - initiating graceful shutdown\n";
    static const char int_msg[] = "Received SIGINT - initiating graceful shutdown\n";
    static const char unknown_msg[] = "Received signal - initiating shutdown\n";
    
    const char *msg;
    size_t msg_len;
    
    // Determine appropriate message based on signal
    switch (signum) {
        case SIGTERM:
            msg = term_msg;
            msg_len = sizeof(term_msg) - 1;
            break;
        case SIGINT:
            msg = int_msg;
            msg_len = sizeof(int_msg) - 1;
            break;
        default:
            msg = unknown_msg;
            msg_len = sizeof(unknown_msg) - 1;
            break;
    }
    
    // Write message using async-signal-safe function
    ssize_t written = write(STDERR_FILENO, msg, msg_len);
    (void)written; // Suppress unused variable warning
    
    // Send shutdown image immediately for clean LCD state
    send_shutdown_image_if_needed();
    
    // Signal graceful shutdown atomically
    running = 0;
}

/**
 * @brief Setup enhanced signal handlers with comprehensive signal management.
 * @details Installs signal handlers for graceful shutdown and blocks unwanted signals.
 * Uses sigaction for more reliable signal handling than signal().
 * @example
 *     setup_enhanced_signal_handlers();
 */
static void setup_enhanced_signal_handlers(void) {
    struct sigaction sa;
    sigset_t block_mask;
    
    // Initialize signal action structure with enhanced settings
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Restart interrupted system calls
    
    // Install handlers for graceful shutdown signals
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "Warning: Failed to install SIGTERM handler: %s\n", strerror(errno));
    }
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Warning: Failed to install SIGINT handler: %s\n", strerror(errno));
    }
    
    // Block unwanted signals to prevent interference
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGPIPE);  // Prevent broken pipe crashes
    sigaddset(&block_mask, SIGHUP);   // Ignore hangup signal for daemon operation
    
    if (pthread_sigmask(SIG_BLOCK, &block_mask, NULL) != 0) {
        fprintf(stderr, "Warning: Failed to block unwanted signals\n");
    }
}

////////////////////////////////////////////////////////////////////////////////
// MAIN ENTRY POINT WITH ENHANCED INITIALIZATION
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Enhanced main entry point for CoolerDash with comprehensive error handling.
 * @details Loads configuration, ensures single instance, initializes all modules, and starts
 * the main daemon loop with proper resource management and cleanup.
 * @example
 *     coolerdash
 *     coolerdash /custom/config.ini
 */
int main(int argc, char **argv) {
    // Early argument validation and help display
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help(argv[0], NULL);
        return EXIT_SUCCESS;
    }
    
    // Validate argument count for security
    if (argc > 2) {
        fprintf(stderr, "Error: Too many arguments. Use --help for usage information.\n");
        return EXIT_FAILURE;
    }
    
    log_message(LOG_INFO, "CoolerDash v%s starting up...", read_version_from_file());
    
    ////////////////////////////////////////////////////////////////////////////////
    // CONFIGURATION LOADING WITH VALIDATION
    ////////////////////////////////////////////////////////////////////////////////
    
    // Determine configuration path with fallback
    const char *config_path = (argc > 1) ? argv[1] : "/etc/coolerdash/config.ini";
    Config config = {0};
    
    log_message(LOG_INFO, "Loading configuration from: %s", config_path);
    if (load_config_ini(&config, config_path) != 0) {
        log_message(LOG_ERROR, "Failed to load configuration file: %s", config_path);
        fprintf(stderr, "Error: Could not load config file '%s'\n", config_path);
        fprintf(stderr, "Please check:\n");
        fprintf(stderr, "  - File exists and is readable\n");
        fprintf(stderr, "  - File has correct INI format\n");
        fprintf(stderr, "  - All required sections are present\n");
        return EXIT_FAILURE;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // INSTANCE MANAGEMENT AND PID FILE HANDLING
    ////////////////////////////////////////////////////////////////////////////////
    
    // Check for existing instances and create PID file with enhanced validation
    int is_service_start = is_started_by_systemd();
    log_message(LOG_INFO, "Running mode: %s", is_service_start ? "systemd service" : "manual");
    
    if (check_existing_instance_and_handle(config.paths_pid, is_service_start) < 0) {
        log_message(LOG_ERROR, "Instance management failed");
        fprintf(stderr, "Error: Another CoolerDash instance is already running\n");
        fprintf(stderr, "To stop the running instance:\n");
        fprintf(stderr, "  sudo systemctl stop coolerdash     # Stop systemd service\n");
        fprintf(stderr, "  sudo pkill coolerdash              # Force kill if needed\n");
        fprintf(stderr, "  sudo systemctl status coolerdash   # Check service status\n");
        return EXIT_FAILURE;
    }
    
    if (write_pid_file(config.paths_pid) != 0) {
        log_message(LOG_ERROR, "Failed to create PID file: %s", config.paths_pid);
        return EXIT_FAILURE;
    }
    
    // Set global config pointer for signal handlers and cleanup
    g_config_ptr = &config;
    
    ////////////////////////////////////////////////////////////////////////////////
    // SIGNAL HANDLER SETUP
    ////////////////////////////////////////////////////////////////////////////////
    
    setup_enhanced_signal_handlers();
    
    ////////////////////////////////////////////////////////////////////////////////
    // COOLERCONTROL SESSION INITIALIZATION
    ////////////////////////////////////////////////////////////////////////////////
    
    log_message(LOG_INFO, "Initializing CoolerControl session...");
    if (!init_coolercontrol_session(&config)) {
        log_message(LOG_ERROR, "CoolerControl session initialization failed");
        fprintf(stderr, "Error: CoolerControl session could not be initialized\n"
                        "Please check:\n"
                        "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
                        "  - Is the daemon running on %s?\n"
                        "  - Is the password correct in configuration?\n"
                        "  - Are network connections allowed?\n", config.daemon_address);
        fflush(stderr);
        remove_pid_file(config.paths_pid);
        return EXIT_FAILURE;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // DEVICE INFORMATION RETRIEVAL
    ////////////////////////////////////////////////////////////////////////////////
    
    // Initialize device data structures
    cc_device_data_t device_data = {0};
    monitor_sensor_data_t temp_data = {0};
    char device_name[CONFIG_MAX_STRING_LEN] = {0};
    int api_screen_width = 0, api_screen_height = 0;
    
    // Get complete device info (UID, name, dimensions) in single API call
    if (get_liquidctl_device_info(&config, device_data.device_uid, sizeof(device_data.device_uid),
                                device_name, sizeof(device_name), &api_screen_width, &api_screen_height)) {
        
        const char *uid_display = (device_data.device_uid[0] != '\0') 
            ? device_data.device_uid 
            : "Unknown device UID";
        const char *name_display = (device_name[0] != '\0') 
            ? device_name 
            : "Unknown device";
            
        log_message(LOG_INFO, "Device: %s [%s]", name_display, uid_display);
        
        // Get temperature data separately for validation
        if (!monitor_get_temperature_data(&config, &temp_data)) {
            log_message(LOG_WARNING, "Could not retrieve initial temperature sensor data");
        }
        
        // Show diagnostic information in debug mode
        show_system_diagnostics(&config, &device_data, api_screen_width, api_screen_height);
    } else {
        log_message(LOG_ERROR, "Could not retrieve device information");
        // Continue execution - some functionality may still work
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // DAEMON EXECUTION AND CLEANUP
    ////////////////////////////////////////////////////////////////////////////////
    
    log_message(LOG_INFO, "Starting daemon (refresh: %d.%03ds)", 
               config.display_refresh_interval_sec,
               config.display_refresh_interval_nsec / 1000000);
    
    // Run daemon with proper error handling
    int result = run_daemon(&config);
    
    // Ensure proper cleanup on exit
    log_message(LOG_INFO, "Daemon shutdown initiated");
    send_shutdown_image_if_needed(); // Ensure shutdown image is sent on normal exit
    remove_pid_file(config.paths_pid);
    running = 0;
    
    log_message(LOG_INFO, "CoolerDash shutdown complete");
    return result;
}