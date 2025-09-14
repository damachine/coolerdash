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
 * @brief Enhanced main entry point for CoolerDash daemon with security and performance optimizations.
 * @details Implements the main daemon logic with improved error handling, secure PID management, and optimized signal processing. Enhanced with input validation, dynamic version loading, and modern C practices.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"
#include "../include/display.h"
#include "../include/monitor.h"

// Security and performance constants
#define DEFAULT_VERSION "unknown"
#define MAX_ERROR_MSG_LEN 512
#define MAX_PID_STR_LEN 32
#define PID_READ_BUFFER_SIZE 64
#define SHUTDOWN_RETRY_COUNT 2
#define VERSION_BUFFER_SIZE 32

/**
 * @brief Global variables for daemon management.
 * @details Used for controlling the main daemon loop and shutdown image logic.
 */
static volatile sig_atomic_t running = 1; // flag whether daemon is running

/**
 * @brief Global logging control.
 * @details Controls whether detailed INFO logs are shown (enabled with --log parameter).
 */
int verbose_logging = 0; // Only ERROR and WARNING by default (exported)

/**
 * @brief Global pointer to configuration.
 * @details Points to the current configuration used by the daemon. Initialized in main().
 */
const Config *g_config_ptr = NULL;

/**
 * @brief Try to open and validate a VERSION file.
 * @details Helper function to reduce code duplication in version file reading.
 */
static FILE *try_open_version_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return NULL;
    }

    struct stat version_stat;
    if (fstat(fd, &version_stat) != 0 || !S_ISREG(version_stat.st_mode))
    {
        close(fd);
        return NULL;
    }

    FILE *fp = fdopen(fd, "r");
    if (!fp)
    {
        close(fd);
        return NULL;
    }

    return fp;
}

/**
 * @brief Read version string from VERSION file with enhanced security.
 * @details Safely reads version from VERSION file with buffer overflow protection and proper validation. Returns fallback version on error.
 */
static const char *read_version_from_file(void)
{
    static char version_buffer[VERSION_BUFFER_SIZE] = {0};
    static int version_loaded = 0;

    // Return cached version if already loaded
    if (version_loaded)
    {
        return version_buffer[0] ? version_buffer : DEFAULT_VERSION;
    }

    // Try to open VERSION file from multiple locations
    FILE *fp = try_open_version_file("VERSION");
    if (!fp)
    {
        fp = try_open_version_file("/opt/coolerdash/VERSION");
    }

    if (!fp)
    {
        log_message(LOG_WARNING, "Could not open VERSION file, using default version");
        cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
        version_loaded = 1;
        return version_buffer;
    }

    // Secure reading with fixed buffer size
    if (!fgets(version_buffer, sizeof(version_buffer), fp))
    {
        log_message(LOG_WARNING, "Could not read VERSION file, using default version");
        cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
    }
    else
    {
        // Remove trailing whitespace and newlines
        version_buffer[strcspn(version_buffer, "\n\r \t")] = '\0';

        // Validate version string (manual bounded length calculation to avoid strnlen portability issues)
        size_t ver_len = 0;
        while (ver_len < 21 && version_buffer[ver_len] != '\0')
        {
            ver_len++;
        }
        if (version_buffer[0] == '\0' || ver_len > 20)
        {
            log_message(LOG_WARNING, "Invalid version format, using default version");
            cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
        }
    }

    fclose(fp);
    version_loaded = 1;
    return version_buffer;
}

/**
 * @brief Safely parse PID from string with validation.
 * @details Uses strtol for secure parsing with proper error checking.
 */
static pid_t safe_parse_pid(const char *pid_str)
{
    if (!pid_str || !pid_str[0])
        return -1;

    char *endptr;
    errno = 0;
    long pid = strtol(pid_str, &endptr, 10);

    // Validation checks
    if (errno != 0 || endptr == pid_str || *endptr != '\0')
        return -1;
    if (pid <= 0 || pid > INT_MAX)
        return -1;

    return (pid_t)pid;
}

/**
 * @brief Detect if we were started by systemd service (not user session).
 * @details Distinguishes between systemd service and user session/terminal.
 */
static int is_started_by_systemd(void)
{
    // Check if running as systemd service by looking at process hierarchy
    // Real systemd services have parent PID 1 and specific unit name
    if (getppid() != 1)
    {
        return 0; // Not direct child of init/systemd
    }

    // Check if we have a proper systemd service unit name
    const char *invocation_id = getenv("INVOCATION_ID");
    if (invocation_id && invocation_id[0])
    {
        // Check if we're running as a proper service (not user session)
        // Services typically have no controlling terminal
        if (!isatty(STDIN_FILENO) && !isatty(STDOUT_FILENO) && !isatty(STDERR_FILENO))
        {
            return 1; // Likely a real systemd service
        }
    }

    return 0; // User session or manual start
}

/**
 * @brief Validate PID file and return file descriptor if valid.
 * @details Opens and validates PID file, returning fd or -1 on error.
 */
static int validate_and_open_pid_file(const char *pid_file)
{
    if (!pid_file || !pid_file[0])
    {
        log_message(LOG_ERROR, "Invalid PID file path provided");
        return -1;
    }

    int fd = open(pid_file, O_RDONLY);
    if (fd == -1)
    {
        return -1; // No PID file exists
    }

    struct stat pid_stat;
    if (fstat(fd, &pid_stat) != 0 || !S_ISREG(pid_stat.st_mode))
    {
        close(fd);
        if (S_ISREG(pid_stat.st_mode) == 0)
        {
            log_message(LOG_WARNING, "PID file '%s' is not a regular file", pid_file);
            unlink(pid_file); // Remove invalid file
        }
        return -1;
    }

    return fd;
}

/**
 * @brief Read PID from file descriptor and validate it.
 * @details Reads and parses PID, returns validated PID or -1 on error.
 */
static pid_t read_and_validate_pid(int fd, const char *pid_file)
{
    FILE *fp = fdopen(fd, "r");
    if (!fp)
    {
        close(fd);
        return -1;
    }

    char pid_buffer[PID_READ_BUFFER_SIZE] = {0};
    if (!fgets(pid_buffer, sizeof(pid_buffer), fp))
    {
        fclose(fp);
        unlink(pid_file); // Remove corrupted PID file
        return -1;
    }

    fclose(fp); // Also closes fd

    // Remove trailing whitespace and validate
    pid_buffer[strcspn(pid_buffer, "\n\r")] = '\0';
    pid_t existing_pid = safe_parse_pid(pid_buffer);

    if (existing_pid <= 0)
    {
        log_message(LOG_WARNING, "Invalid PID in file, removing stale PID file");
        unlink(pid_file);
        return -1;
    }

    return existing_pid;
}

/**
 * @brief Check if process with given PID is running.
 * @details Uses kill(pid, 0) to check process existence.
 */
static int is_process_running(pid_t pid)
{
    if (kill(pid, 0) == 0)
    {
        return 1; // Process exists
    }
    else if (errno == EPERM)
    {
        return 1; // Process exists but no permission to signal it
    }
    return 0; // Process doesn't exist
}

/**
 * @brief Check if another instance of CoolerDash is running with secure PID validation.
 * @details Uses secure file reading and PID validation.
 */
static int check_existing_instance_and_handle(const char *pid_file, int is_service_start)
{
    (void)is_service_start; // Mark as intentionally unused

    int fd = validate_and_open_pid_file(pid_file);
    if (fd == -1)
    {
        return 0; // No valid PID file, no running instance
    }

    pid_t existing_pid = read_and_validate_pid(fd, pid_file);
    if (existing_pid <= 0)
    {
        return 0; // Invalid or corrupted PID file handled
    }

    if (is_process_running(existing_pid))
    {
        if (errno == EPERM)
        {
            log_message(LOG_ERROR, "Another instance may be running (PID %d) - insufficient permissions to verify", existing_pid);
        }
        else
        {
            log_message(LOG_ERROR, "Another instance is already running (PID %d)", existing_pid);
        }
        return -1;
    }

    // Process doesn't exist, remove stale PID file
    log_message(LOG_INFO, "Removing stale PID file (process %d no longer exists)", existing_pid);
    unlink(pid_file);
    return 0;
}

/**
 * @brief Create directory for PID file if needed.
 * @details Ensures the parent directory exists for the PID file.
 */
static int ensure_pid_directory(const char *pid_file)
{
    char *dir_path = strdup(pid_file);
    if (!dir_path)
    {
        log_message(LOG_ERROR, "Memory allocation failed for directory path");
        return -1;
    }

    char *last_slash = strrchr(dir_path, '/');
    if (last_slash)
    {
        *last_slash = '\0';
        if (mkdir(dir_path, 0755) == -1 && errno != EEXIST)
        {
            log_message(LOG_WARNING, "Could not create PID directory '%s': %s", dir_path, strerror(errno));
        }
    }

    free(dir_path);
    return 0;
}

/**
 * @brief Create temporary PID file with proper permissions.
 * @details Creates temporary file for atomic PID file creation.
 */
static int create_temp_pid_file(const char *pid_file, char *temp_file, size_t temp_size)
{
    int ret = snprintf(temp_file, temp_size, "%s.tmp", pid_file);
    if (ret >= (int)temp_size || ret < 0)
    {
        log_message(LOG_ERROR, "PID file path too long");
        return -1;
    }

    int fd = open(temp_file, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1)
    {
        log_message(LOG_ERROR, "Could not create temporary PID file '%s': %s", temp_file, strerror(errno));
        return -1;
    }

    return fd;
}

/**
 * @brief Write PID data to file descriptor.
 * @details Writes current PID to file with proper validation and flushing.
 */
static int write_pid_data(int fd, const char *temp_file)
{
    FILE *f = fdopen(fd, "w");
    if (!f)
    {
        log_message(LOG_ERROR, "Could not convert file descriptor to FILE*: %s", strerror(errno));
        close(fd);
        unlink(temp_file);
        return -1;
    }

    pid_t current_pid = getpid();
    if (fprintf(f, "%d\n", current_pid) < 0)
    {
        log_message(LOG_ERROR, "Could not write PID to temporary file '%s': %s", temp_file, strerror(errno));
        fclose(f);
        unlink(temp_file);
        return -1;
    }

    if (fflush(f) != 0)
    {
        log_message(LOG_ERROR, "Could not flush PID to temporary file '%s': %s", temp_file, strerror(errno));
        fclose(f);
        unlink(temp_file);
        return -1;
    }

    if (fclose(f) != 0)
    {
        log_message(LOG_ERROR, "Could not close temporary PID file '%s': %s", temp_file, strerror(errno));
        unlink(temp_file);
        return -1;
    }

    return 0;
}

/**
 * @brief Atomically move temporary PID file to final location.
 * @details Performs atomic rename operation for PID file creation.
 */
static int finalize_pid_file(const char *temp_file, const char *pid_file)
{
    if (rename(temp_file, pid_file) != 0)
    {
        log_message(LOG_ERROR, "Could not rename temporary PID file to '%s': %s", pid_file, strerror(errno));
        unlink(temp_file);
        return -1;
    }

    pid_t current_pid = getpid();
    log_message(LOG_STATUS, "PID file: %s (PID: %d)", pid_file, current_pid);
    return 0;
}

/**
 * @brief Write current PID to file with enhanced security and error checking.
 * @details Creates PID file with proper permissions and atomic write operation.
 */
static int write_pid_file(const char *pid_file)
{
    if (!pid_file || !pid_file[0])
    {
        log_message(LOG_ERROR, "Invalid PID file path provided");
        return -1;
    }

    if (ensure_pid_directory(pid_file) != 0)
    {
        return -1;
    }

    char temp_file[PATH_MAX];
    int fd = create_temp_pid_file(pid_file, temp_file, sizeof(temp_file));
    if (fd == -1)
    {
        return -1;
    }

    if (write_pid_data(fd, temp_file) != 0)
    {
        return -1;
    }

    return finalize_pid_file(temp_file, pid_file);
}

/**
 * @brief Remove PID file with enhanced error handling.
 * @details Securely removes the PID file with proper error reporting.
 */
static void remove_pid_file(const char *pid_file)
{
    if (!pid_file || !pid_file[0])
        return;

    if (unlink(pid_file) == 0)
    {
        log_message(LOG_INFO, "PID file removed");
    }
    else if (errno != ENOENT)
    {
        log_message(LOG_WARNING, "Could not remove PID file '%s': %s", pid_file, strerror(errno));
    }
}

/**
 * @brief Remove generated image file during cleanup.
 * @details Securely removes the generated PNG image file with proper error reporting.
 */
static void remove_image_file(const char *image_file)
{
    if (!image_file || !image_file[0])
        return;

    if (unlink(image_file) == 0)
    {
        log_message(LOG_INFO, "Image file removed");
    }
    else if (errno != ENOENT)
    {
        log_message(LOG_WARNING, "Could not remove image file '%s': %s", image_file, strerror(errno));
    }
}

/**
 * @brief Enhanced help display with improved formatting and security information.
 * @details Prints comprehensive usage information and security recommendations.
 */
static void show_help(const char *program_name, const Config *config)
{
    (void)config; // Mark parameter as intentionally unused

    if (!program_name)
        program_name = "coolerdash";

    const char *version = read_version_from_file();

    printf("================================================================================\n");
    printf("CoolerDash v%s - LCD Dashboard for CoolerControl\n", version);
    printf("================================================================================\n\n");
    printf("DESCRIPTION:\n");
    printf("  A high-performance daemon that displays CPU and GPU temperatures on LCD screens\n");
    printf("  connected via CoolerControl.\n\n");
    printf("USAGE:\n");
    printf("  %s [OPTIONS] [CONFIG_PATH]\n\n", program_name);
    printf("OPTIONS:\n");
    printf("  -h, --help     Show this help message and exit\n");
    printf("  --log          Enable detailed INFO logging for debugging\n\n");
    printf("EXAMPLES:\n");
    printf("  sudo systemctl start coolerdash           # Start as system service (recommended)\n");
    printf("  %s                                # Manual start with default config\n", program_name);
    printf("  %s --log                          # Start with detailed logging enabled\n", program_name);
    printf("  %s /custom/config.ini             # Start with custom configuration\n\n", program_name);
    printf("FILES:\n");
    printf("  /usr/bin/coolerdash                       # Main program executable\n");
    printf("  /opt/coolerdash/                          # Installation directory\n");
    printf("  /etc/coolerdash/config.ini                # Default configuration file\n");
    printf("  /tmp/coolerdash.pid                       # PID file (auto-managed, user-accessible)\n");
    printf("  /var/log/syslog                           # Log output (when run as service)\n\n");
    printf("SECURITY:\n");
    printf("  - Runs as dedicated user 'coolerdash' for enhanced security\n");
    printf("  - Communicates via CoolerControl's HTTP API (no direct device access)\n");
    printf("  - Automatically manages single-instance enforcement\n");
    printf("For detailed documentation: man coolerdash\n");
    printf("Project repository: https://github.com/damachine/coolerdash\n");
    printf("================================================================================\n");
}

/**
 * @brief Display system information for diagnostics.
 * @details Shows display configuration, API validation results, and refresh interval settings for system diagnostics.
 */
static void show_system_diagnostics(const Config *config, int api_width, int api_height)
{
    if (!config)
        return;

    // Display configuration with API validation integrated
    if (api_width > 0 && api_height > 0)
    {
        if (api_width != config->display_width || api_height != config->display_height)
        {
            log_message(LOG_STATUS, "Display configuration: (%dx%d pixels)",
                        config->display_width, config->display_height);
            log_message(LOG_WARNING, "API reports different dimensions: (%dx%d pixels)",
                        api_width, api_height);
        }
        else
        {
            log_message(LOG_STATUS, "Display configuration: (%dx%d pixels) (Device confirmed)",
                        config->display_width, config->display_height);
        }
    }
    else
    {
        log_message(LOG_STATUS, "Display configuration: (%dx%d pixels) (Device confirmed)",
                    config->display_width, config->display_height);
    }

    log_message(LOG_STATUS, "Refresh interval: %d.%03d seconds",
                config->display_refresh_interval_sec,
                config->display_refresh_interval_nsec / 1000000);
}

/**
 * @brief Validate shutdown image file existence.
 * @details Check if shutdown image exists and is a regular file.
 */
static int is_shutdown_image_valid(const char *image_path)
{
    if (!image_path || !image_path[0])
    {
        return 0;
    }

    struct stat file_stat;
    return (stat(image_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode));
}

/**
 * @brief Send shutdown image to LCD device.
 * @details Sends the shutdown image twice for better reliability.
 */
static void send_shutdown_image(const Config *config, const char *image_path, const char *device_uid)
{
    send_image_to_lcd(config, image_path, device_uid);
    send_image_to_lcd(config, image_path, device_uid); // Send twice for better reliability
}

/**
 * @brief Turn off LCD by sending fallback image with brightness 0.
 * @details Creates temporary config with brightness 0 to turn off display.
 */
static void turn_off_lcd_display(const Config *config, const char *device_uid)
{
    Config temp_config = *config;
    temp_config.lcd_brightness = 0;

    const char *fallback_image = config->paths_image_coolerdash;
    if (fallback_image && fallback_image[0])
    {
        send_shutdown_image(&temp_config, fallback_image, device_uid);
    }
}

/**
 * @brief Send shutdown image if needed or turn off LCD if image is missing.
 * @details Checks if shutdown image should be sent to LCD device and performs the transmission if conditions are met.
 */
static void send_shutdown_image_if_needed(void)
{
    // Basic validation
    if (!is_session_initialized() || !g_config_ptr)
    {
        return;
    }

    // Get device UID
    char device_uid[128];
    if (!get_liquidctl_data(g_config_ptr, device_uid, sizeof(device_uid), NULL, 0, NULL, NULL) || !device_uid[0])
    {
        return;
    }

    const char *shutdown_image_path = g_config_ptr->paths_image_shutdown;

    if (is_shutdown_image_valid(shutdown_image_path))
    {
        send_shutdown_image(g_config_ptr, shutdown_image_path, device_uid);
    }
    else
    {
        log_message(LOG_WARNING, "Shutdown image '%s' not found, turning off LCD display",
                    shutdown_image_path ? shutdown_image_path : "unspecified");
        turn_off_lcd_display(g_config_ptr, device_uid);
    }
}

/**
 * @brief Write signal message to stderr safely.
 * @details Uses async-signal-safe write for signal handler logging.
 */
static void write_signal_message(int signum)
{
    static const char term_msg[] = "Received SIGTERM - initiating graceful shutdown\n";
    static const char int_msg[] = "Received SIGINT - initiating graceful shutdown\n";
    static const char unknown_msg[] = "Received signal - initiating shutdown\n";

    const char *msg;
    size_t msg_len;

    switch (signum)
    {
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

    ssize_t written = write(STDERR_FILENO, msg, msg_len);
    (void)written; // Suppress unused variable warning
}

/**
 * @brief Perform signal-safe cleanup operations.
 * @details Cleans up PID and image files using signal-safe functions only.
 */
static void signal_safe_cleanup(void)
{
    if (g_config_ptr)
    {
        // Remove PID file - array address is never NULL, just check if path is set
        if (g_config_ptr->paths_pid[0])
        {
            unlink(g_config_ptr->paths_pid);
        }
        // Remove generated image file - array address is never NULL, just check if path is set
        if (g_config_ptr->paths_image_coolerdash[0])
        {
            unlink(g_config_ptr->paths_image_coolerdash);
        }
    }
}

/**
 * @brief Enhanced signal handler with atomic operations and secure shutdown.
 * @details Signal-safe implementation using only async-signal-safe functions.
 */
static void handle_shutdown_signal(int signum)
{
    write_signal_message(signum);
    send_shutdown_image_if_needed();
    signal_safe_cleanup();
    running = 0; // Signal graceful shutdown atomically
}

/**
 * @brief Setup enhanced signal handlers with comprehensive signal management.
 * @details Installs signal handlers for graceful shutdown and blocks unwanted signals.
 */
static void setup_enhanced_signal_handlers(void)
{
    struct sigaction sa;
    sigset_t block_mask;

    // Initialize signal action structure with enhanced settings
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Restart interrupted system calls

    // Install handlers for graceful shutdown signals
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        log_message(LOG_WARNING, "Failed to install SIGTERM handler: %s", strerror(errno));
    }

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        log_message(LOG_WARNING, "Failed to install SIGINT handler: %s", strerror(errno));
    }

    // Block unwanted signals to prevent interference
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGPIPE); // Prevent broken pipe crashes
    sigaddset(&block_mask, SIGHUP);  // Ignore hangup signal for daemon operation

    if (pthread_sigmask(SIG_BLOCK, &block_mask, NULL) != 0)
    {
        log_message(LOG_WARNING, "Failed to block unwanted signals");
    }
}

/**
 * @brief Calculate next execution time for daemon loop.
 * @details Adds interval to current time with overflow protection.
 */
static void calculate_next_time(struct timespec *next_time, const struct timespec *interval)
{
    next_time->tv_sec += interval->tv_sec;
    next_time->tv_nsec += interval->tv_nsec;
    if (next_time->tv_nsec >= 1000000000L)
    {
        next_time->tv_sec++;
        next_time->tv_nsec -= 1000000000L;
    }
}

/**
 * @brief Sleep until specified absolute time.
 * @details Handles sleep interruptions and errors properly.
 */
static void sleep_until_time(const struct timespec *target_time)
{
    int sleep_result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, target_time, NULL);
    if (sleep_result != 0 && sleep_result != EINTR)
    {
        log_message(LOG_WARNING, "Sleep interrupted: %s", strerror(sleep_result));
    }
}

/**
 * @brief Enhanced main daemon loop with improved timing and error handling.
 * @details Runs the main loop with precise timing, optimized sleep, and graceful error recovery.
 */
static int run_daemon(const Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid configuration provided to daemon");
        return -1;
    }

    const struct timespec interval = {
        .tv_sec = config->display_refresh_interval_sec,
        .tv_nsec = config->display_refresh_interval_nsec};

    struct timespec next_time;
    if (clock_gettime(CLOCK_MONOTONIC, &next_time) != 0)
    {
        log_message(LOG_ERROR, "Failed to get current time: %s", strerror(errno));
        return -1;
    }

    while (running)
    {
        calculate_next_time(&next_time, &interval);
        draw_combined_image(config);
        sleep_until_time(&next_time);
    }

    return 0;
}

/**
 * @brief Manage instance handling and PID file creation.
 * @details Checks for existing instances and creates PID file with validation.
 */
static int setup_instance_management(const Config *config)
{
    // Check for existing instances and create PID file with enhanced validation
    int is_service_start = is_started_by_systemd();
    log_message(LOG_INFO, "Running mode: %s", is_service_start ? "systemd service" : "manual");

    if (check_existing_instance_and_handle(config->paths_pid, is_service_start) < 0)
    {
        log_message(LOG_ERROR, "Instance management failed");
        fprintf(stderr, "Error: Another CoolerDash instance is already running\n");
        fprintf(stderr, "To stop the running instance:\n");
        fprintf(stderr, "  sudo systemctl stop coolerdash     # Stop systemd service\n");
        fprintf(stderr, "  sudo pkill coolerdash              # Force kill if needed\n");
        fprintf(stderr, "  sudo systemctl status coolerdash   # Check service status\n");
        return -1;
    }

    if (write_pid_file(config->paths_pid) != 0)
    {
        log_message(LOG_ERROR, "Failed to create PID file: %s", config->paths_pid);
        return -1;
    }

    return 0;
}

/**
 * @brief Perform comprehensive cleanup on shutdown.
 * @details Centralizes all cleanup operations to reduce duplication.
 */
static void perform_final_cleanup(const Config *config)
{
    log_message(LOG_INFO, "Daemon shutdown initiated");
    send_shutdown_image_if_needed(); // Ensure shutdown image is sent
    remove_pid_file(config->paths_pid);
    remove_image_file(config->paths_image_coolerdash);
    running = 0;
    log_message(LOG_INFO, "CoolerDash shutdown complete");
}

/**
 * @brief Print CoolerControl connection troubleshooting information.
 * @details Helper function to reduce code duplication for error messages.
 */
static void print_coolercontrol_troubleshooting(const Config *config)
{
    fprintf(stderr, "Error: CoolerControl session could not be initialized\n"
                    "Please check:\n"
                    "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
                    "  - Is the daemon running on %s?\n"
                    "  - Is the password correct in configuration?\n"
                    "  - Are network connections allowed?\n",
            config->daemon_address);
    fflush(stderr);
}

/**
 * @brief Initialize CoolerControl session and device cache.
 * @details Initializes session, device cache, and validates device connectivity.
 */
static int initialize_coolercontrol_systems(Config *config)
{
    // Initialize CoolerControl session
    log_message(LOG_STATUS, "Initializing CoolerControl session...");
    if (!init_coolercontrol_session(config))
    {
        log_message(LOG_ERROR, "CoolerControl session initialization failed");
        print_coolercontrol_troubleshooting(config);
        return -1;
    }

    // Initialize device cache once at startup for optimal performance
    log_message(LOG_STATUS, "CoolerDash initializing device cache...\n");
    if (!init_device_cache(config))
    {
        log_message(LOG_ERROR, "Failed to initialize device cache");
        print_coolercontrol_troubleshooting(config);
        return -1;
    }

    return 0;
}

/**
 * @brief Validate device connectivity and log device information.
 * @details Gets device information and tests sensor connectivity.
 */
static void validate_device_connectivity(const Config *config)
{
    // Initialize device data structures
    char device_uid[128] = {0};
    monitor_sensor_data_t temp_data = {0};
    char device_name[CONFIG_MAX_STRING_LEN] = {0};
    int api_screen_width = 0, api_screen_height = 0;

    // Get complete device info from cache (no API call)
    if (get_liquidctl_data(config, device_uid, sizeof(device_uid),
                           device_name, sizeof(device_name), &api_screen_width, &api_screen_height))
    {
        const char *uid_display = (device_uid[0] != '\0') ? device_uid : "Unknown device UID";
        const char *name_display = (device_name[0] != '\0') ? device_name : "Unknown device";

        log_message(LOG_STATUS, "Device: %s [%s]", name_display, uid_display);

        // Get temperature data separately for validation and log sensor detection status
        if (get_temperature_monitor_data(config, &temp_data))
        {
            if (temp_data.temp_cpu > 0.0f || temp_data.temp_gpu > 0.0f)
            {
                log_message(LOG_STATUS, "Sensor values successfully detected");
            }
            else
            {
                log_message(LOG_WARNING, "Sensor detection issues - temperature values not available");
            }
        }
        else
        {
            log_message(LOG_WARNING, "Sensor detection issues - check CoolerControl connection");
        }

        // Show diagnostic information in debug mode
        show_system_diagnostics(config, api_screen_width, api_screen_height);
    }
    else
    {
        log_message(LOG_ERROR, "Could not retrieve device information");
        // Continue execution - some functionality may still work
    }
}

/**
 * @brief Initialize configuration from file.
 * @details Loads configuration from the specified path with comprehensive error handling.
 */
static int initialize_configuration(const char *config_path, Config *config)
{
    log_message(LOG_STATUS, "Loading configuration...");
    if (load_config(config_path, config) != 0)
    {
        log_message(LOG_ERROR, "Failed to load configuration file: %s", config_path);
        fprintf(stderr, "Error: Could not load config file '%s'\n", config_path);
        fprintf(stderr, "Please check:\n");
        fprintf(stderr, "  - File exists and is readable\n");
        fprintf(stderr, "  - File has correct INI format\n");
        fprintf(stderr, "  - All required sections are present\n");
        return -1;
    }
    return 0;
}

/**
 * @brief Parse command line arguments.
 * @details Processes command line arguments and returns the configuration path.
 */
static const char *parse_command_line_args(int argc, char **argv)
{
    const char *config_path = "/etc/coolerdash/config.ini"; // Default config path

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            show_help(argv[0], NULL);
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[i], "--log") == 0)
        {
            verbose_logging = 1; // Enable detailed INFO logging
        }
        else if (argv[i][0] != '-')
        {
            // This is the config path (no dash prefix)
            config_path = argv[i];
        }
        else
        {
            fprintf(stderr, "Error: Unknown option '%s'. Use --help for usage information.\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    return config_path;
}

/**
 * @brief Enhanced main entry point for CoolerDash with comprehensive error handling.
 * @details Loads configuration, ensures single instance, initializes all modules, and starts the main daemon loop.
 */
int main(int argc, char **argv)
{
    // Parse command line arguments
    const char *config_path = parse_command_line_args(argc, argv);

    log_message(LOG_STATUS, "CoolerDash v%s starting up...", read_version_from_file());

    // Load configuration
    Config config = {0};
    if (initialize_configuration(config_path, &config) != 0)
    {
        return EXIT_FAILURE;
    }

    // Setup instance management and PID file
    if (setup_instance_management(&config) != 0)
    {
        return EXIT_FAILURE;
    }

    // Set global config pointer for signal handlers and cleanup
    g_config_ptr = &config;

    // Setup enhanced signal handlers with comprehensive error handling
    setup_enhanced_signal_handlers();

    // Initialize CoolerControl session and device systems
    if (initialize_coolercontrol_systems(&config) != 0)
    {
        remove_pid_file(config.paths_pid);
        return EXIT_FAILURE;
    }

    // Validate device connectivity and log device information
    validate_device_connectivity(&config);

    log_message(LOG_STATUS, "Starting daemon");

    // Run daemon with proper error handling
    int result = run_daemon(&config);

    // Ensure proper cleanup on exit
    perform_final_cleanup(&config);
    return result;
}