/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Main entry point and daemon lifecycle.
 * @details Signal handling, PID management, version loading, main loop.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "device/config.h"
#include "mods/circle.h"
#include "mods/display.h"
#include "srv/cc_conf.h"
#include "srv/cc_main.h"
#include "srv/cc_sensor.h"

// Security and performance constants
#define DEFAULT_VERSION "unknown"
#define VERSION_BUFFER_SIZE 32

/**
 * @brief Global variables for daemon management.
 * @details Used for controlling the main daemon loop and shutdown image logic.
 */
static volatile sig_atomic_t running = 1;       // flag whether daemon is running
static volatile sig_atomic_t reload_config = 0; // flag for SIGHUP config reload

/**
 * @brief File-scope storage for config reload.
 * @details Preserved across reload so reload_daemon_config() can re-read
 * config.json and re-apply CLI overrides.
 */
static const char *s_config_path = NULL;
static char s_display_mode_override[16] = {0};

/**
 * @brief Global logging control.
 * @details Controls whether detailed INFO logs are shown (enabled with --log
 * parameter).
 */
int verbose_logging = 0; // Only ERROR and WARNING by default (exported)

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
static void validate_version_string(char *version_buffer, size_t buffer_size)
{
    // Remove trailing whitespace and newlines
    version_buffer[strcspn(version_buffer, "\n\r \t")] = '\0';

    // Validate version string (manual bounded length calculation to avoid strnlen
    // portability issues)
    size_t ver_len = 0;
    while (ver_len < 21 && version_buffer[ver_len] != '\0')
    {
        ver_len++;
    }
    if (version_buffer[0] == '\0' || ver_len > 20)
    {
        log_message(LOG_WARNING, "Invalid version format, using default version");
        cc_safe_strcpy(version_buffer, buffer_size, DEFAULT_VERSION);
    }
}

/**
 * @brief Read version string from VERSION file with enhanced security.
 * @details Safely reads version from VERSION file with buffer overflow
 * protection and proper validation. Returns fallback version on error.
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

    // Try to read from VERSION file
    FILE *fp = fopen("VERSION", "r");
    if (!fp)
    {
        // Try alternative path for installed version
        fp = fopen("/etc/coolercontrol/plugins/coolerdash/VERSION", "r");
    }

    if (!fp)
    {
        log_message(LOG_WARNING,
                    "Could not open VERSION file, using default version");
        cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
        version_loaded = 1;
        return version_buffer;
    }

    // Secure reading with fixed buffer size
    if (!fgets(version_buffer, sizeof(version_buffer), fp))
    {
        log_message(LOG_WARNING,
                    "Could not read VERSION file, using default version");
        cc_safe_strcpy(version_buffer, sizeof(version_buffer), DEFAULT_VERSION);
    }
    else
    {
        validate_version_string(version_buffer, sizeof(version_buffer));
    }

    fclose(fp);
    version_loaded = 1;
    return version_buffer;
}

/**
 * @brief Detect if we were started by CoolerControl plugin system.
 * @details Checks INVOCATION_ID environment variable set by systemd/CoolerControl.
 */
static int is_started_as_plugin(void)
{
    const char *invocation_id = getenv("INVOCATION_ID");
    return (invocation_id && invocation_id[0]) ? 1 : 0;
}

/**
 * @brief Enhanced help display with improved formatting and security
 * information.
 * @details Prints comprehensive usage information and security recommendations.
 */
static void show_help(const char *program_name)
{
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
    printf("  --develop         Developer mode: enable verbose logging\n\n");
    printf("DISPLAY MODES:\n");
    printf(
        "  dual              Default mode - shows CPU and GPU simultaneously\n");
    printf("  circle            Alternating mode - switches between CPU/GPU "
           "every 2.5 seconds\n");
    printf("                    Configure via config.json [display] "
           "mode=dual|circle or CLI flags\n\n");
    printf("EXAMPLES:\n");
    printf("  sudo systemctl restart coolercontrold     # Restart CoolerControl "
           "(reloads plugin)\n");
    printf("  %s                                # Standalone start with default "
           "config (dual mode)\n",
           program_name);
    printf("  %s --circle                       # Standalone with circle mode "
           "(alternating display)\n",
           program_name);
    printf("  %s --dual --verbose               # Force dual mode with detailed "
           "logging\n",
           program_name);
    printf("  %s /custom/config.json            # Start with custom "
           "configuration\n\n",
           program_name);
    printf("FILES:\n");
    printf("  /usr/libexec/coolerdash/coolerdash            # Main executable\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/         # Plugin data directory\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/config.json # Configuration "
           "file\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/index.html # Web UI settings\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/manifest.toml # Plugin manifest\n");
    printf("  /tmp/coolerdash.pid                       # PID file "
           "(auto-managed)\n");
    printf("  journalctl -u coolercontrold.service      # View plugin logs\n\n");
    printf("PLUGIN MODE:\n");
    printf("  - Managed by CoolerControl (coolercontrold.service)\n");
    printf("  - Runs as CoolerControl plugin user (isolated environment)\n");
    printf("  - Communicates via CoolerControl's HTTP API (no direct device "
           "access)\n");
    printf("  - Automatically started/stopped with CoolerControl\n");
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
                                    int api_height)
{
    if (!config)
        return;

    // Display configuration with API validation integrated
    if (api_width > 0 && api_height > 0)
    {
        if (api_width != config->display_width ||
            api_height != config->display_height)
        {
            log_message(LOG_STATUS, "Display configuration: (%dx%d pixels)",
                        config->display_width, config->display_height);
            log_message(LOG_WARNING,
                        "API reports different dimensions: (%dx%d pixels)", api_width,
                        api_height);
        }
        else
        {
            log_message(LOG_STATUS,
                        "Display configuration: (%dx%d pixels) (Device confirmed)",
                        config->display_width, config->display_height);
        }
    }
    else
    {
        log_message(LOG_STATUS,
                    "Display configuration: (%dx%d pixels) (Device confirmed)",
                    config->display_width, config->display_height);
    }

    log_message(LOG_STATUS, "Refresh interval: %.2f seconds",
                config->display_refresh_interval);
}

/**
 * @brief Enhanced signal handler with atomic operations and secure shutdown.
 * @details Signal-safe implementation using only async-signal-safe functions.
 */
static void handle_shutdown_signal(int signum)
{
    // Use only async-signal-safe functions in signal handlers
    static const char term_msg[] =
        "Received SIGTERM - initiating graceful shutdown\n";
    static const char int_msg[] =
        "Received SIGINT - initiating graceful shutdown\n";
    static const char quit_msg[] =
        "Received SIGQUIT - initiating graceful shutdown\n";
    static const char unknown_msg[] = "Received signal - initiating shutdown\n";

    const char *msg;
    size_t msg_len;

    // Determine appropriate message based on signal
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
    case SIGQUIT:
        msg = quit_msg;
        msg_len = sizeof(quit_msg) - 1;
        break;
    default:
        msg = unknown_msg;
        msg_len = sizeof(unknown_msg) - 1;
        break;
    }

    // Write message using async-signal-safe function
    ssize_t written = write(STDERR_FILENO, msg, msg_len);
    (void)written; // Suppress unused variable warning

    // Signal graceful shutdown atomically
    // Note: File cleanup (PID, images) is handled in perform_cleanup()
    running = 0;
}

/**
 * @brief Signal handler for SIGHUP (config reload).
 * @details Sets the reload flag so the main loop re-reads config.json.
 * Only uses async-signal-safe functions.
 */
static void handle_reload_signal(int signum)
{
    (void)signum;
    static const char msg[] = "Received SIGHUP - scheduling config reload\n";
    ssize_t written = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)written;
    reload_config = 1;
}

/**
 * @brief Setup enhanced signal handlers with comprehensive signal management.
 * @details Installs signal handlers for graceful shutdown and blocks unwanted
 * signals.
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
        log_message(LOG_WARNING, "Failed to install SIGTERM handler: %s",
                    strerror(errno));
    }

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        log_message(LOG_WARNING, "Failed to install SIGINT handler: %s",
                    strerror(errno));
    }

    if (sigaction(SIGQUIT, &sa, NULL) == -1)
    {
        log_message(LOG_WARNING, "Failed to install SIGQUIT handler: %s",
                    strerror(errno));
    }

    // Install SIGHUP handler for config reload
    struct sigaction sa_reload;
    memset(&sa_reload, 0, sizeof(sa_reload));
    sa_reload.sa_handler = handle_reload_signal;
    sigemptyset(&sa_reload.sa_mask);
    sa_reload.sa_flags = SA_RESTART;

    if (sigaction(SIGHUP, &sa_reload, NULL) == -1)
    {
        log_message(LOG_WARNING, "Failed to install SIGHUP handler: %s",
                    strerror(errno));
    }

    // Block unwanted signals to prevent interference
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGPIPE); // Prevent broken pipe crashes

    if (pthread_sigmask(SIG_BLOCK, &block_mask, NULL) != 0)
    {
        log_message(LOG_WARNING, "Failed to block unwanted signals");
    }
}

/**
 * @brief Reload configuration from config.json (SIGHUP handler).
 * @details Resets all module state, re-reads config.json, and re-initializes
 * CoolerControl session and device cache. On failure, logs error but the
 * daemon continues running with the previous configuration values.
 */
static void reload_daemon_config(Config *config)
{
    log_message(LOG_STATUS, "SIGHUP received — reloading configuration...");

    // 1. Tear down module state (reverse order of init)
    cleanup_sensor_curl_handle();
    reset_coolercontrol_session();
    reset_device_cache();
    reset_circle_state();

    // 2. Re-read config.json into the existing struct
    load_plugin_config(config, s_config_path);

    // 3. Re-apply CLI display mode override if it was set at startup
    if (s_display_mode_override[0] != '\0')
    {
        cc_safe_strcpy(config->display_mode, sizeof(config->display_mode),
                       s_display_mode_override);
        log_message(LOG_INFO, "Display mode overridden by CLI: %s",
                    config->display_mode);
    }

    // 4. Re-initialize CoolerControl session with potentially new token/address
    if (!init_coolercontrol_session(config))
    {
        log_message(LOG_ERROR,
                    "Config reload: session re-init failed — continuing with degraded state");
        return;
    }

    // 5. Re-populate device cache
    if (!init_device_cache(config))
    {
        log_message(LOG_ERROR,
                    "Config reload: device cache re-init failed — continuing with degraded state");
        return;
    }

    // 6. Update LCD dimensions from device if config has width/height=0
    update_config_from_device(config);

    log_message(LOG_STATUS, "Configuration reloaded successfully");
}

/**
 * @brief Enhanced main daemon loop with improved timing and error handling.
 * @details Runs the main loop with precise timing, optimized sleep, and
 * graceful error recovery. Handles SIGHUP for live config reload.
 */
static int run_daemon(Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid configuration provided to daemon");
        return -1;
    }

    // Convert float seconds to timespec (e.g., 2.50 -> tv_sec=2,
    // tv_nsec=500000000)
    long interval_sec = (long)config->display_refresh_interval;
    long interval_nsec =
        (long)((config->display_refresh_interval - interval_sec) * 1000000000);

    struct timespec interval = {.tv_sec = interval_sec,
                                .tv_nsec = interval_nsec};

    struct timespec next_time;
    if (clock_gettime(CLOCK_MONOTONIC, &next_time) != 0)
    {
        log_message(LOG_ERROR, "Failed to get current time: %s", strerror(errno));
        return -1;
    }

    while (running)
    {
        // Check for pending config reload (SIGHUP)
        if (reload_config)
        {
            reload_config = 0;
            reload_daemon_config(config);

            // Recalculate interval from potentially changed refresh_interval
            interval_sec = (long)config->display_refresh_interval;
            interval_nsec = (long)((config->display_refresh_interval -
                                    interval_sec) *
                                   1000000000);
            interval.tv_sec = interval_sec;
            interval.tv_nsec = interval_nsec;
        }

        // Calculate next execution time with overflow protection
        next_time.tv_sec += interval.tv_sec;
        next_time.tv_nsec += interval.tv_nsec;
        if (next_time.tv_nsec >= 1000000000L)
        {
            next_time.tv_sec++;
            next_time.tv_nsec -= 1000000000L;
        }

        // Execute main rendering task
        draw_display_image(config);

        // Sleep until absolute time with error handling
        int sleep_result =
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
        if (sleep_result != 0 && sleep_result != EINTR)
        {
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
                                   char *display_mode_override)
{
    const char *config_path = "/etc/coolercontrol/plugins/coolerdash/config.json";
    display_mode_override[0] = '\0'; // Initialize as empty

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            show_help(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[i], "-v") == 0 ||
                 strcmp(argv[i], "--verbose") == 0)
        {
            verbose_logging = 1;
        }
        else if (strcmp(argv[i], "--dual") == 0)
        {
            cc_safe_strcpy(display_mode_override, 16, "dual");
        }
        else if (strcmp(argv[i], "--circle") == 0)
        {
            cc_safe_strcpy(display_mode_override, 16, "circle");
        }
        else if (strcmp(argv[i], "--develop") == 0)
        {
            verbose_logging = 1; // Developer mode implies verbose logging
        }
        else if (argv[i][0] != '-')
        {
            config_path = argv[i];
        }
        else
        {
            fprintf(stderr,
                    "Error: Unknown option '%s'. Use --help for usage information.\n",
                    argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    return config_path;
}

/**
 * @brief Verify plugin directory write permissions for generated images.
 * @details Ensures the plugin directory is writable by the current user.
 */
static int verify_plugin_dir_permissions(const char *plugin_dir)
{
    if (!plugin_dir || !plugin_dir[0])
        return 1;

    if (access(plugin_dir, W_OK) != 0)
    {
        log_message(LOG_WARNING,
                    "Plugin directory not writable: %s (errno: %d) - Generated images may fail",
                    plugin_dir, errno);
        return 0;
    }

    log_message(LOG_INFO, "Plugin directory verified: %s", plugin_dir);
    return 1;
}

/**
 * @brief Initialize configuration from plugin config.json.
 * @details Loads config using unified plugin.c system:
 *          1. Initialize defaults (hardcoded)
 *          2. Try to load config.json (overrides defaults)
 *          3. Apply remaining defaults for missing fields
 */
static int initialize_config_and_instance(const char *config_path,
                                          Config *config)
{
    // Load configuration from config.json (or defaults if not found)
    int json_loaded = load_plugin_config(config, config_path);

    if (!json_loaded)
    {
        log_message(LOG_INFO, "Using hardcoded defaults (no config.json found)");
    }

    int is_plugin_mode = is_started_as_plugin();
    log_message(LOG_INFO, "Running mode: %s",
                is_plugin_mode ? "CoolerControl plugin" : "standalone");

    // Verify plugin directory write permissions for image generation
    if (!verify_plugin_dir_permissions(config->paths_images))
    {
        log_message(LOG_ERROR, "Failed to verify plugin directory permissions");
        return 0;
    }

    return 1;
}

/**
 * @brief Initialize CoolerControl services.
 * @details Initializes session and device cache.
 */
static int initialize_coolercontrol_services(const Config *config)
{
    if (!init_coolercontrol_session(config))
    {
        log_message(LOG_ERROR, "CoolerControl session initialization failed");
        fprintf(stderr,
                "Error: CoolerControl session could not be initialized\n"
                "Please check:\n"
                "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
                "  - Is the daemon running on %s?\n"
                "  - Is a valid access token configured?\n"
                "  - Are network connections allowed?\n",
                config->daemon_address);
        fflush(stderr);
        return -1;
    }

    if (!init_device_cache(config))
    {
        log_message(LOG_ERROR, "Failed to initialize device cache");
        fprintf(stderr,
                "Error: CoolerControl session could not be initialized\n"
                "Please check:\n"
                "  - Is coolercontrold running? (systemctl status coolercontrold)\n"
                "  - Is the daemon running on %s?\n"
                "  - Is a valid access token configured?\n"
                "  - Are network connections allowed?\n",
                config->daemon_address);
        return -1;
    }

    return 0;
}

/**
 * @brief Initialize and validate device information.
 * @details Retrieves device info and validates sensors.
 */
static void initialize_device_info(Config *config)
{
    char device_uid[128] = {0};
    monitor_sensor_data_t temp_data = {0};
    char device_name[CONFIG_MAX_STRING_LEN] = {0};
    int api_screen_width = 0, api_screen_height = 0;

    if (!get_cached_lcd_device_data(config, device_uid, sizeof(device_uid),
                                    device_name, sizeof(device_name),
                                    &api_screen_width, &api_screen_height))
    {
        log_message(LOG_ERROR, "Could not retrieve device information");
        return;
    }

    update_config_from_device(config);

    const char *uid_display =
        (device_uid[0] != '\0') ? device_uid : "Unknown device UID";
    const char *name_display =
        (device_name[0] != '\0') ? device_name : "Unknown device";

    log_message(LOG_STATUS, "Device: %s [%s]", name_display, uid_display);

    if (get_sensor_monitor_data(config, &temp_data))
    {
        if (temp_data.sensor_count > 0)
        {
            log_message(LOG_STATUS, "Sensor values successfully detected (%d sensors)",
                        temp_data.sensor_count);
        }
        else
        {
            log_message(LOG_WARNING,
                        "Sensor detection issues - no sensor values available");
        }
    }
    else
    {
        log_message(LOG_WARNING,
                    "Sensor detection issues - check CoolerControl connection");
    }

    show_system_diagnostics(config, api_screen_width, api_screen_height);
}

/**
 * @brief Perform cleanup operations.
 * @details Closes CoolerControl session and frees resources.
 * The LCD image file is intentionally kept on disk so that CoolerControl
 * can still find it when applying saved settings on next startup
 * (coolerdash is started after CC applies settings).
 * Shutdown image is handled by CC4 natively via register_shutdown_image_with_cc().
 */
static void perform_cleanup(const Config *config)
{
    (void)config;
    log_message(LOG_INFO, "Daemon shutdown initiated");

    // Close CoolerControl session and free resources
    cleanup_coolercontrol_session();
    cleanup_sensor_curl_handle();

    running = 0;
    log_message(LOG_INFO, "CoolerDash shutdown complete");
}

/**
 * @brief Enhanced main entry point for CoolerDash with comprehensive error
 * handling.
 * @details Loads configuration, ensures single instance, initializes all
 * modules, and starts the main daemon loop.
 */
int main(int argc, char **argv)
{
    char display_mode_override[16] = {0};
    const char *config_path = parse_arguments(argc, argv, display_mode_override);

    // Store in file-scope statics for SIGHUP reload
    s_config_path = config_path;
    cc_safe_strcpy(s_display_mode_override, sizeof(s_display_mode_override),
                   display_mode_override);

    log_message(LOG_STATUS, "CoolerDash v%s starting up...",
                read_version_from_file());

    Config config = {0};
    log_message(LOG_STATUS, "Loading configuration...");

    if (!initialize_config_and_instance(config_path, &config))
    {
        return EXIT_FAILURE;
    }

    // Apply CLI display mode override if provided
    if (s_display_mode_override[0] != '\0')
    {
        cc_safe_strcpy(config.display_mode, sizeof(config.display_mode),
                       s_display_mode_override);
        log_message(LOG_INFO, "Display mode overridden by CLI: %s",
                    config.display_mode);
    }

    g_config_ptr = &config;
    setup_enhanced_signal_handlers();

    log_message(LOG_STATUS, "Initializing CoolerControl session...");
    if (initialize_coolercontrol_services(&config) != 0)
    {
        return EXIT_FAILURE;
    }

    log_message(LOG_STATUS, "CoolerDash initializing device cache...\n");
    initialize_device_info(&config);

    // Render initial image immediately so the PNG exists on disk
    // before CC applies saved LCD settings (avoids startup race condition)
    log_message(LOG_INFO, "Rendering initial display image...");
    draw_display_image(&config);

    /* CC4: Register shutdown.png once at startup so CoolerControl displays
     * it natively when the CC daemon stops (MR !417 / CC 4.0). */
    if (config.paths_image_shutdown[0])
    {
        char device_uid[128] = {0};
        if (get_cached_lcd_device_data(&config, device_uid,
                                       sizeof(device_uid), NULL, 0, NULL,
                                       NULL) &&
            device_uid[0])
        {
            register_shutdown_image_with_cc(&config,
                                            config.paths_image_shutdown,
                                            device_uid);
        }
    }

    log_message(LOG_STATUS, "Starting daemon");
    int result = run_daemon(&config);

    perform_cleanup(&config);
    return result;
}
