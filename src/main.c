/**
 * -----------------------------------------------------------------------------
 * Created by: damachine (christkue79 at gmail dot com)
 * Website: https://github.com/damachine/coolerdash
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Enhanced main entry point for CoolerDash daemon with security and
 * performance optimizations.
 * @details Implements the main daemon logic with improved error handling,
 * secure PID management, and optimized signal processing. Enhanced with input
 * validation, dynamic version loading, and modern C practices.
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
#include "device/sys.h"
#include "device/usr.h"
#include "mods/display.h"
#include "srv/cc_conf.h"
#include "srv/cc_main.h"
#include "srv/cc_sensor.h"

// Security and performance constants
#define DEFAULT_VERSION "unknown"
#define MAX_ERROR_MSG_LEN 512
#define MAX_PID_STR_LEN 32
#define PID_READ_BUFFER_SIZE 64
#define SHUTDOWN_RETRY_COUNT 2
#define VERSION_BUFFER_SIZE 32

// Define O_NOFOLLOW if not defined (for portability)
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

/**
 * @brief Global variables for daemon management.
 * @details Used for controlling the main daemon loop and shutdown image logic.
 */
static volatile sig_atomic_t running = 1; // flag whether daemon is running

/**
 * @brief Global logging control.
 * @details Controls whether detailed INFO logs are shown (enabled with --log
 * parameter).
 */
int verbose_logging = 0; // Only ERROR and WARNING by default (exported)
// Developer/testing flag: when set (via --develop), force displays to be
// treated as circular
int force_display_circular = 0;

/**
 * @brief Global pointer to configuration.
 * @details Points to the current configuration used by the daemon. Initialized
 * in main().
 */
const Config *g_config_ptr = NULL;

/**
 * @brief Validate and sanitize version string.
 * @details Checks version string validity and sets default if invalid.
 */
static void validate_version_string(char *version_buffer, size_t buffer_size) {
  // Remove trailing whitespace and newlines
  version_buffer[strcspn(version_buffer, "\n\r \t")] = '\0';

  // Validate version string (manual bounded length calculation to avoid strnlen
  // portability issues)
  size_t ver_len = 0;
  while (ver_len < 21 && version_buffer[ver_len] != '\0') {
    ver_len++;
  }
  if (version_buffer[0] == '\0' || ver_len > 20) {
    log_message(LOG_WARNING, "Invalid version format, using default version");
    cc_safe_strcpy(version_buffer, buffer_size, DEFAULT_VERSION);
  }
}

/**
 * @brief Read version string from VERSION file with enhanced security.
 * @details Safely reads version from VERSION file with buffer overflow
 * protection and proper validation. Returns fallback version on error.
 */
static const char *read_version_from_file(void) {
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
    log_message(LOG_WARNING,
                "Could not open VERSION file, using default version");
    cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
    version_loaded = 1;
    return version_buffer;
  }

  // Secure reading with fixed buffer size
  if (!fgets(version_buffer, sizeof(version_buffer), fp)) {
    log_message(LOG_WARNING,
                "Could not read VERSION file, using default version");
    cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
  } else {
    validate_version_string(version_buffer, sizeof(version_buffer));
  }

  fclose(fp);
  version_loaded = 1;
  return version_buffer;
}

/**
 * @brief Safely parse PID from string with validation.
 * @details Uses strtol for secure parsing with proper error checking.
 */
static pid_t safe_parse_pid(const char *pid_str) {
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
static int is_started_by_systemd(void) {
  // Check if running as systemd service by looking at process hierarchy
  // Real systemd services have parent PID 1 and specific unit name
  if (getppid() != 1) {
    return 0; // Not direct child of init/systemd
  }

  // Check if we have a proper systemd service unit name
  const char *invocation_id = getenv("INVOCATION_ID");
  if (invocation_id && invocation_id[0]) {
    // Check if we're running as a proper service (not user session)
    // Services typically have no controlling terminal
    if (!isatty(STDIN_FILENO) && !isatty(STDOUT_FILENO) &&
        !isatty(STDERR_FILENO)) {
      return 1; // Likely a real systemd service
    }
  }

  return 0; // User session or manual start
}

/**
 * @brief Check if another instance of CoolerDash is running with secure PID
 * validation.
 * @details Uses secure file reading and PID validation.
 */
static int check_existing_instance_and_handle(const char *pid_file) {
  if (!pid_file || !pid_file[0]) {
    log_message(LOG_ERROR, "Invalid PID file path provided");
    return -1;
  }

  FILE *fp = fopen(pid_file, "r");
  if (!fp)
    return 0; // No PID file exists, no running instance

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
    log_message(LOG_ERROR, "Another instance is already running (PID %d)",
                existing_pid);
    return -1;
  } else if (errno == EPERM) {
    log_message(LOG_ERROR,
                "Another instance may be running (PID %d) - insufficient "
                "permissions to verify",
                existing_pid);
    return -1;
  }

  // Process doesn't exist, remove stale PID file
  log_message(LOG_INFO, "Removing stale PID file (process %d no longer exists)",
              existing_pid);
  unlink(pid_file);
  return 0;
}

/**
 * @brief Create parent directory for PID file.
 * @details Ensures the directory exists with proper permissions.
 */
static int create_pid_directory(const char *pid_file) {
  char *dir_path = strdup(pid_file);
  if (!dir_path) {
    log_message(LOG_ERROR, "Memory allocation failed for directory path");
    return -1;
  }

  char *last_slash = strrchr(dir_path, '/');
  if (last_slash) {
    *last_slash = '\0';
    if (mkdir(dir_path, 0755) == -1 && errno != EEXIST) {
      log_message(LOG_WARNING, "Could not create PID directory '%s': %s",
                  dir_path, strerror(errno));
    }
  }
  free(dir_path);
  return 0;
}

/**
 * @brief Open temporary PID file with security checks.
 * @details Creates temporary file with atomic operations and validates it's a
 * regular file.
 */
static int open_temp_pid_file(const char *temp_file) {
  int fd = open(temp_file, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
  if (fd == -1) {
    log_message(LOG_ERROR, "Could not create temporary PID file '%s': %s",
                temp_file, strerror(errno));
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) == 0) {
    if (!S_ISREG(st.st_mode)) {
      close(fd);
      log_message(LOG_ERROR, "PID file is not a regular file: %s", temp_file);
      return -1;
    }
  }
  return fd;
}

/**
 * @brief Write PID to file descriptor.
 * @details Converts fd to FILE* and writes PID with error checking.
 */
static int write_pid_to_file(int fd, const char *temp_file) {
  FILE *f = fdopen(fd, "w");
  if (!f) {
    log_message(LOG_ERROR, "Could not convert file descriptor to FILE*: %s",
                strerror(errno));
    close(fd);
    unlink(temp_file);
    return -1;
  }

  pid_t current_pid = getpid();
  if (fprintf(f, "%d\n", current_pid) < 0) {
    log_message(LOG_ERROR, "Could not write PID to temporary file '%s': %s",
                temp_file, strerror(errno));
    fclose(f);
    unlink(temp_file);
    return -1;
  }

  if (fclose(f) != 0) {
    log_message(LOG_ERROR, "Could not close temporary PID file '%s': %s",
                temp_file, strerror(errno));
    unlink(temp_file);
    return -1;
  }
  return 0;
}

/**
 * @brief Create temporary file path for PID file.
 * @details Generates a temporary file path with proper bounds checking.
 */
static int create_temp_pid_path(const char *pid_file, char *temp_file,
                                size_t temp_file_size) {
  int ret = snprintf(temp_file, temp_file_size, "%s.tmp", pid_file);
  if (ret >= (int)temp_file_size || ret < 0) {
    log_message(LOG_ERROR, "PID file path too long");
    return -1;
  }
  return 0;
}

/**
 * @brief Write current PID to file with enhanced security and error checking.
 * @details Creates PID file with proper permissions and atomic write operation.
 */
static int write_pid_file(const char *pid_file) {
  if (!pid_file || !pid_file[0]) {
    log_message(LOG_ERROR, "Invalid PID file path provided");
    return -1;
  }

  if (create_pid_directory(pid_file) != 0) {
    return -1;
  }

  char temp_file[PATH_MAX];
  if (create_temp_pid_path(pid_file, temp_file, sizeof(temp_file)) != 0) {
    return -1;
  }

  int fd = open_temp_pid_file(temp_file);
  if (fd == -1) {
    return -1;
  }

  if (write_pid_to_file(fd, temp_file) != 0) {
    return -1;
  }

  if (rename(temp_file, pid_file) != 0) {
    log_message(LOG_ERROR, "Could not rename temporary PID file to '%s': %s",
                pid_file, strerror(errno));
    unlink(temp_file);
    return -1;
  }

  log_message(LOG_STATUS, "PID file: %s (PID: %d)", pid_file, getpid());
  return 0;
}

/**
 * @brief Remove PID file with enhanced error handling.
 * @details Securely removes the PID file with proper error reporting.
 */
static void remove_pid_file(const char *pid_file) {
  if (!pid_file || !pid_file[0])
    return;

  if (unlink(pid_file) == 0) {
    log_message(LOG_INFO, "PID file removed");
  } else if (errno != ENOENT) {
    log_message(LOG_WARNING, "Could not remove PID file '%s': %s", pid_file,
                strerror(errno));
  }
}

/**
 * @brief Remove generated image file during cleanup.
 * @details Securely removes the generated PNG image file with proper error
 * reporting.
 */
static void remove_image_file(const char *image_file) {
  if (!image_file || !image_file[0])
    return;

  if (unlink(image_file) == 0) {
    log_message(LOG_INFO, "Image file removed");
  } else if (errno != ENOENT) {
    log_message(LOG_WARNING, "Could not remove image file '%s': %s", image_file,
                strerror(errno));
  }
}

/**
 * @brief Enhanced help display with improved formatting and security
 * information.
 * @details Prints comprehensive usage information and security recommendations.
 */
static void show_help(const char *program_name) {
  if (!program_name)
    program_name = "coolerdash";

  const char *version = read_version_from_file();

  printf("====================================================================="
         "===========\n");
  printf("CoolerDash v%s - LCD Dashboard for CoolerControl\n", version);
  printf("====================================================================="
         "===========\n\n");
  printf("DESCRIPTION:\n");
  printf("  A high-performance daemon that displays CPU and GPU temperatures "
         "on LCD screens\n");
  printf("  connected via CoolerControl.\n\n");
  printf("USAGE:\n");
  printf("  %s [OPTIONS] [CONFIG_PATH]\n\n", program_name);
  printf("OPTIONS:\n");
  printf("  -h, --help        Show this help message and exit\n");
  printf("  -v, --verbose     Enable verbose logging (shows detailed INFO "
         "messages)\n");
  printf(
      "  --dual            Force dual display mode (CPU+GPU simultaneously)\n");
  printf("  --circle          Force circle mode (alternating CPU/GPU every 2.5 "
         "seconds)\n");
  printf("  --develop         Developer: force display to be treated as "
         "circular for testing\n\n");
  printf("DISPLAY MODES:\n");
  printf(
      "  dual              Default mode - shows CPU and GPU simultaneously\n");
  printf("  circle            Alternating mode - switches between CPU/GPU "
         "every 2.5 seconds\n");
  printf("                    Configure via config.ini [display] "
         "mode=dual|circle or CLI flags\n\n");
  printf("EXAMPLES:\n");
  printf("  sudo systemctl start coolerdash           # Start as system "
         "service (recommended)\n");
  printf("  %s                                # Manual start with default "
         "config (dual mode)\n",
         program_name);
  printf("  %s --circle                       # Start with circle mode "
         "(alternating display)\n",
         program_name);
  printf("  %s --dual --verbose               # Force dual mode with detailed "
         "logging\n",
         program_name);
  printf("  %s -v                             # Short form: enable verbose "
         "logging\n",
         program_name);
  printf("  %s /custom/config.ini             # Start with custom "
         "configuration\n\n",
         program_name);
  printf("FILES:\n");
  printf("  /usr/bin/coolerdash                       # Main program "
         "executable\n");
  printf(
      "  /opt/coolerdash/                          # Installation directory\n");
  printf("  /etc/coolerdash/config.ini                # Default configuration "
         "file\n");
  printf("  /tmp/coolerdash.pid                       # PID file "
         "(auto-managed, user-accessible)\n");
  printf("  /var/log/syslog                           # Log output (when run "
         "as service)\n\n");
  printf("SECURITY:\n");
  printf("  - Runs as dedicated user 'coolerdash' for enhanced security\n");
  printf("  - Communicates via CoolerControl's HTTP API (no direct device "
         "access)\n");
  printf("  - Automatically manages single-instance enforcement\n");
  printf("For detailed documentation: man coolerdash\n");
  printf("Project repository: https://github.com/damachine/coolerdash\n");
  printf("====================================================================="
         "===========\n");
}

/**
 * @brief Display system information for diagnostics.
 * @details Shows display configuration, API validation results, and refresh
 * interval settings for system diagnostics.
 */
static void show_system_diagnostics(const Config *config, int api_width,
                                    int api_height) {
  if (!config)
    return;

  // Display configuration with API validation integrated
  if (api_width > 0 && api_height > 0) {
    if (api_width != config->display_width ||
        api_height != config->display_height) {
      log_message(LOG_STATUS, "Display configuration: (%dx%d pixels)",
                  config->display_width, config->display_height);
      log_message(LOG_WARNING,
                  "API reports different dimensions: (%dx%d pixels)", api_width,
                  api_height);
    } else {
      log_message(LOG_STATUS,
                  "Display configuration: (%dx%d pixels) (Device confirmed)",
                  config->display_width, config->display_height);
    }
  } else {
    log_message(LOG_STATUS,
                "Display configuration: (%dx%d pixels) (Device confirmed)",
                config->display_width, config->display_height);
  }

  log_message(LOG_STATUS, "Refresh interval: %.2f seconds",
              config->display_refresh_interval);
}

/**
 * @brief Validate session and configuration for shutdown image.
 * @details Checks if session is initialized and configuration is valid.
 */
static int validate_shutdown_conditions(void) {
  if (!is_session_initialized() || !g_config_ptr) {
    return 0;
  }
  return 1;
}

/**
 * @brief Get device UID for shutdown operation.
 * @details Retrieves device UID from liquidctl data.
 */
static int get_device_uid_for_shutdown(char *device_uid, size_t uid_size) {
  if (!get_liquidctl_data(g_config_ptr, device_uid, uid_size, NULL, 0, NULL,
                          NULL) ||
      !device_uid[0]) {
    return 0;
  }
  return 1;
}

/**
 * @brief Handle missing shutdown image by turning off LCD.
 * @details Creates temporary config with brightness 0 to turn off display.
 */
static void handle_missing_shutdown_image(const char *shutdown_image_path,
                                          const char *device_uid) {
  log_message(LOG_WARNING,
              "Shutdown image '%s' not found, turning off LCD display",
              shutdown_image_path);

  Config temp_config = *g_config_ptr;
  temp_config.lcd_brightness = 0;

  const char *fallback_image = g_config_ptr->paths_image_coolerdash;
  if (fallback_image && fallback_image[0]) {
    send_image_to_lcd(&temp_config, fallback_image, device_uid);
  }
}

/**
 * @brief Send shutdown image if needed or turn off LCD if image is missing.
 * @details Checks if shutdown image should be sent to LCD device and performs
 * the transmission if conditions are met. If shutdown image is missing, sets
 * LCD brightness to 0 to turn off the display.
 */
static void send_shutdown_image_if_needed(void) {
  if (!validate_shutdown_conditions()) {
    return;
  }

  char device_uid[128];
  if (!get_device_uid_for_shutdown(device_uid, sizeof(device_uid))) {
    return;
  }

  const char *shutdown_image_path = g_config_ptr->paths_image_shutdown;
  if (!shutdown_image_path || !shutdown_image_path[0]) {
    return;
  }

  FILE *image_file = fopen(shutdown_image_path, "r");
  if (image_file) {
    fclose(image_file);
    send_image_to_lcd(g_config_ptr, shutdown_image_path, device_uid);
  } else {
    handle_missing_shutdown_image(shutdown_image_path, device_uid);
  }
}

/**
 * @brief Enhanced signal handler with atomic operations and secure shutdown.
 * @details Signal-safe implementation using only async-signal-safe functions.
 */
static void handle_shutdown_signal(int signum) {
  // Use only async-signal-safe functions in signal handlers
  static const char term_msg[] =
      "Received SIGTERM - initiating graceful shutdown\n";
  static const char int_msg[] =
      "Received SIGINT - initiating graceful shutdown\n";
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

  // Clean up temporary files (PID and image) - signal-safe operations
  if (g_config_ptr) {
    // Remove PID file - array address is never NULL, just check if path is set
    if (g_config_ptr->paths_pid[0]) {
      unlink(g_config_ptr->paths_pid);
    }
    // Remove generated image file - array address is never NULL, just check if
    // path is set
    if (g_config_ptr->paths_image_coolerdash[0]) {
      unlink(g_config_ptr->paths_image_coolerdash);
    }
  }

  // Signal graceful shutdown atomically
  running = 0;
}

/**
 * @brief Setup enhanced signal handlers with comprehensive signal management.
 * @details Installs signal handlers for graceful shutdown and blocks unwanted
 * signals.
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
    log_message(LOG_WARNING, "Failed to install SIGTERM handler: %s",
                strerror(errno));
  }

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    log_message(LOG_WARNING, "Failed to install SIGINT handler: %s",
                strerror(errno));
  }

  // Block unwanted signals to prevent interference
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGPIPE); // Prevent broken pipe crashes
  sigaddset(&block_mask, SIGHUP);  // Ignore hangup signal for daemon operation

  if (pthread_sigmask(SIG_BLOCK, &block_mask, NULL) != 0) {
    log_message(LOG_WARNING, "Failed to block unwanted signals");
  }
}

/**
 * @brief Enhanced main daemon loop with improved timing and error handling.
 * @details Runs the main loop with precise timing, optimized sleep, and
 * graceful error recovery.
 */
static int run_daemon(const Config *config) {
  if (!config) {
    log_message(LOG_ERROR, "Invalid configuration provided to daemon");
    return -1;
  }

  // Convert float seconds to timespec (e.g., 2.50 -> tv_sec=2,
  // tv_nsec=500000000)
  const long interval_sec = (long)config->display_refresh_interval;
  const long interval_nsec =
      (long)((config->display_refresh_interval - interval_sec) * 1000000000);

  const struct timespec interval = {.tv_sec = interval_sec,
                                    .tv_nsec = interval_nsec};

  struct timespec next_time;
  if (clock_gettime(CLOCK_MONOTONIC, &next_time) != 0) {
    log_message(LOG_ERROR, "Failed to get current time: %s", strerror(errno));
    return -1;
  }

  while (running) {
    // Calculate next execution time with overflow protection
    next_time.tv_sec += interval.tv_sec;
    next_time.tv_nsec += interval.tv_nsec;
    if (next_time.tv_nsec >= 1000000000L) {
      next_time.tv_sec++;
      next_time.tv_nsec -= 1000000000L;
    }

    // Execute main rendering task
    draw_display_image(config);

    // Sleep until absolute time with error handling
    int sleep_result =
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
    if (sleep_result != 0 && sleep_result != EINTR) {
      log_message(LOG_WARNING, "Sleep interrupted: %s", strerror(sleep_result));
    }
  }

  return 0;
}

/**
 * @brief Parse command line arguments.
 * @details Processes command line options and returns config path.
 */
static const char *parse_arguments(int argc, char **argv,
                                   char *display_mode_override) {
  const char *config_path = "/etc/coolerdash/config.ini";
  display_mode_override[0] = '\0'; // Initialize as empty

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      show_help(argv[0]);
      exit(EXIT_SUCCESS);
    } else if (strcmp(argv[i], "-v") == 0 ||
               strcmp(argv[i], "--verbose") == 0) {
      verbose_logging = 1;
    } else if (strcmp(argv[i], "--dual") == 0) {
      cc_safe_strcpy(display_mode_override, 16, "dual");
    } else if (strcmp(argv[i], "--circle") == 0) {
      cc_safe_strcpy(display_mode_override, 16, "circle");
    } else if (strcmp(argv[i], "--develop") == 0) {
      force_display_circular = 1;
      verbose_logging = 1; // Developer mode implies verbose logging
    } else if (argv[i][0] != '-') {
      config_path = argv[i];
    } else {
      fprintf(stderr,
              "Error: Unknown option '%s'. Use --help for usage information.\n",
              argv[i]);
      exit(EXIT_FAILURE);
    }
  }

  return config_path;
}

/**
 * @brief Initialize configuration and instance management.
 * @details Loads config using new three-stage approach:
 *          1. Initialize system defaults (always available)
 *          2. Load user config if present (overrides defaults)
 *          3. Apply system defaults to any missing fields
 */
static int initialize_config_and_instance(const char *config_path,
                                          Config *config) {
  // Stage 1: Initialize system defaults
  init_system_defaults(config);

  // Stage 2: Load user configuration (if file exists)
  int user_config_result = load_user_config(config_path, config);
  if (user_config_result < 0) {
    log_message(LOG_ERROR, "Failed to parse configuration file: %s",
                config_path);
    fprintf(stderr, "Error: Could not parse config file '%s'\n", config_path);
    fprintf(stderr, "Please check:\n");
    fprintf(stderr, "  - File has correct INI format\n");
    fprintf(stderr, "  - All sections and keys are valid\n");
    return -1;
  }

  // Stage 3: Apply system defaults to any fields not set by user config
  apply_system_defaults(config);

  /* Apply CLI overrides (developer/testing) */
  if (force_display_circular) {
    config->force_display_circular = 1;
    log_message(LOG_INFO, "Developer override: forcing circular display "
                          "detection (via --develop)");
  }

  int is_service_start = is_started_by_systemd();
  log_message(LOG_INFO, "Running mode: %s",
              is_service_start ? "systemd service" : "manual");

  if (check_existing_instance_and_handle(config->paths_pid) < 0) {
    log_message(LOG_ERROR, "Instance management failed");
    fprintf(stderr, "Error: Another CoolerDash instance is already running\n");
    fprintf(stderr, "To stop the running instance:\n");
    fprintf(stderr,
            "  sudo systemctl stop coolerdash     # Stop systemd service\n");
    fprintf(stderr,
            "  sudo pkill coolerdash              # Force kill if needed\n");
    fprintf(stderr,
            "  sudo systemctl status coolerdash   # Check service status\n");
    return -1;
  }

  if (write_pid_file(config->paths_pid) != 0) {
    log_message(LOG_ERROR, "Failed to create PID file: %s", config->paths_pid);
    return -1;
  }

  return 0;
}

/**
 * @brief Initialize CoolerControl services.
 * @details Initializes session and device cache.
 */
static int initialize_coolercontrol_services(const Config *config) {
  if (!init_coolercontrol_session(config)) {
    log_message(LOG_ERROR, "CoolerControl session initialization failed");
    fprintf(stderr,
            "Error: CoolerControl session could not be initialized\n"
            "Please check:\n"
            "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
            "  - Is the daemon running on %s?\n"
            "  - Is the password correct in configuration?\n"
            "  - Are network connections allowed?\n",
            config->daemon_address);
    fflush(stderr);
    remove_pid_file(config->paths_pid);
    return -1;
  }

  if (!init_device_cache(config)) {
    log_message(LOG_ERROR, "Failed to initialize device cache");
    fprintf(stderr,
            "Error: CoolerControl session could not be initialized\n"
            "Please check:\n"
            "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
            "  - Is the daemon running on %s?\n"
            "  - Is the password correct in configuration?\n"
            "  - Are network connections allowed?\n",
            config->daemon_address);
    remove_pid_file(config->paths_pid);
    return -1;
  }

  return 0;
}

/**
 * @brief Initialize and validate device information.
 * @details Retrieves device info and validates sensors.
 */
static void initialize_device_info(Config *config) {
  char device_uid[128] = {0};
  monitor_sensor_data_t temp_data = {0};
  char device_name[CONFIG_MAX_STRING_LEN] = {0};
  int api_screen_width = 0, api_screen_height = 0;

  if (!get_liquidctl_data(config, device_uid, sizeof(device_uid), device_name,
                          sizeof(device_name), &api_screen_width,
                          &api_screen_height)) {
    log_message(LOG_ERROR, "Could not retrieve device information");
    return;
  }

  update_config_from_device(config);

  const char *uid_display =
      (device_uid[0] != '\0') ? device_uid : "Unknown device UID";
  const char *name_display =
      (device_name[0] != '\0') ? device_name : "Unknown device";

  log_message(LOG_STATUS, "Device: %s [%s]", name_display, uid_display);

  if (get_temperature_monitor_data(config, &temp_data)) {
    if (temp_data.temp_cpu > 0.0f || temp_data.temp_gpu > 0.0f) {
      log_message(LOG_STATUS, "Sensor values successfully detected");
    } else {
      log_message(LOG_WARNING,
                  "Sensor detection issues - temperature values not available");
    }
  } else {
    log_message(LOG_WARNING,
                "Sensor detection issues - check CoolerControl connection");
  }

  show_system_diagnostics(config, api_screen_width, api_screen_height);
}

/**
 * @brief Perform cleanup operations.
 * @details Removes PID and image files, sends shutdown image, closes
 * CoolerControl session.
 */
static void perform_cleanup(const Config *config) {
  log_message(LOG_INFO, "Daemon shutdown initiated");
  send_shutdown_image_if_needed();

  // Close CoolerControl session and free resources
  cleanup_coolercontrol_session();

  remove_pid_file(config->paths_pid);
  remove_image_file(config->paths_image_coolerdash);
  running = 0;
  log_message(LOG_INFO, "CoolerDash shutdown complete");
}

/**
 * @brief Enhanced main entry point for CoolerDash with comprehensive error
 * handling.
 * @details Loads configuration, ensures single instance, initializes all
 * modules, and starts the main daemon loop.
 */
int main(int argc, char **argv) {
  char display_mode_override[16] = {0};
  const char *config_path = parse_arguments(argc, argv, display_mode_override);

  log_message(LOG_STATUS, "CoolerDash v%s starting up...",
              read_version_from_file());

  Config config = {0};
  log_message(LOG_STATUS, "Loading configuration...");

  if (initialize_config_and_instance(config_path, &config) != 0) {
    return EXIT_FAILURE;
  }

  // Apply CLI display mode override if provided
  if (display_mode_override[0] != '\0') {
    cc_safe_strcpy(config.display_mode, sizeof(config.display_mode),
                   display_mode_override);
    log_message(LOG_INFO, "Display mode overridden by CLI: %s",
                config.display_mode);
  }

  g_config_ptr = &config;
  setup_enhanced_signal_handlers();

  log_message(LOG_STATUS, "Initializing CoolerControl session...");
  if (initialize_coolercontrol_services(&config) != 0) {
    return EXIT_FAILURE;
  }

  log_message(LOG_STATUS, "CoolerDash initializing device cache...\n");
  initialize_device_info(&config);

  log_message(LOG_STATUS, "Starting daemon");
  int result = run_daemon(&config);

  perform_cleanup(&config);
  return result;
}