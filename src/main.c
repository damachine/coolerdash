/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Daemon entry point: signal handling, version, main loop.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <jansson.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "device/config.h"
#include "device/hwreport.h"
#include "mods/display.h"
#include "srv/cc_conf.h"
#include "srv/cc_main.h"
#include "srv/cc_sensor.h"

// Security and performance constants
#define DEFAULT_VERSION "unknown"
#define VERSION_BUFFER_SIZE 32
#define COOLERDASH_LOCK_NAME ".coolerdash.lock"
#define GH_UPDATE_URL "https://api.github.com/repos/damachine/coolerdash/releases/latest"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_config = 0;

static const char *s_config_path = NULL;
static char s_display_mode_override[16] = {0};
static int s_instance_fd = -1;
static char s_instance_lock_path[CONFIG_MAX_PATH_LEN] = {0};

typedef struct CliOptions
{
    const char *config_path;
    char display_mode_override[16];
    int hardware_report;
    HardwareReportOptions report;
} CliOptions;

int verbose_logging = 0;

const Config *g_config_ptr = NULL;

/** @brief Resolve configured shutdown image path with silent fallback to shutdown.png. */
static const char *resolve_shutdown_image_path(const Config *config)
{
    if (!config)
        return NULL;

    if (config->paths_image_shutdown[0] != '\0')
    {
        if (access(config->paths_image_shutdown, R_OK) == 0)
            return config->paths_image_shutdown;
    }

    if (access(DEFAULT_SHUTDOWN_IMAGE_PATH, R_OK) == 0)
        return DEFAULT_SHUTDOWN_IMAGE_PATH;

    log_message(LOG_WARNING,
                "No readable shutdown image available. Checked configured path and default shutdown image: %s",
                DEFAULT_SHUTDOWN_IMAGE_PATH);
    return NULL;
}

/** @brief Strip whitespace and validate version string. Sets default on error. */
static void validate_version_string(char *version_buffer, size_t buffer_size)
{
    version_buffer[strcspn(version_buffer, "\n\r \t")] = '\0';

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

/** @brief Read version from VERSION file; returns cached value on repeat calls. */
static const char *read_version_from_file(void)
{
    static char version_buffer[VERSION_BUFFER_SIZE] = {0};
    static int version_loaded = 0;

    if (version_loaded)
    {
        return version_buffer[0] ? version_buffer : DEFAULT_VERSION;
    }

    FILE *fp = fopen("VERSION", "r");
    if (!fp)
        fp = fopen("/etc/coolercontrol/plugins/coolerdash/VERSION", "r");

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

/** @brief Parse "X.Y.Z" semver into integers. Returns 1 on success. */
static int parse_semver(const char *s, int *major, int *minor, int *patch)
{
    if (!s)
        return 0;
    if (*s == 'v' || *s == 'V')
        s++;
    return sscanf(s, "%d.%d.%d", major, minor, patch) == 3;
}

/** @brief Returns >0 if b is newer than a, 0 if equal, <0 if older. */
static int semver_newer(const char *a, const char *b)
{
    int ma, mia, pa, mb, mib, pb;
    if (!parse_semver(a, &ma, &mia, &pa) || !parse_semver(b, &mb, &mib, &pb))
        return 0;
    if (mb != ma)
        return mb > ma ? 1 : -1;
    if (mib != mia)
        return mib > mia ? 1 : -1;
    return pb > pa ? 1 : (pb < pa ? -1 : 0);
}

/** @brief Query GitHub Releases API and log if a newer version exists. */
static void check_for_update(const char *current_version)
{
    if (!current_version || current_version[0] == '\0')
        return;

    CURL *curl = curl_easy_init();
    if (!curl)
        return;

    http_response buf = {0};
    if (!cc_init_response_buffer(&buf, 2048))
    {
        curl_easy_cleanup(curl);
        return;
    }

    char user_agent[64];
    snprintf(user_agent, sizeof(user_agent), "CoolerDash/%s", current_version);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");

    curl_easy_setopt(curl, CURLOPT_URL, GH_UPDATE_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (res == CURLE_OK && http_code == 200 && buf.data)
    {
        json_error_t jerr;
        json_t *root = json_loads(buf.data, 0, &jerr);
        if (root)
        {
            const json_t *tag = json_object_get(root, "tag_name");
            if (json_is_string(tag))
            {
                const char *latest = json_string_value(tag);
                if (semver_newer(current_version, latest) > 0)
                {
                    log_message(LOG_STATUS,
                                "Update available: v%s -> %s  "
                                "https://github.com/damachine/coolerdash/releases",
                                current_version, latest);
                }
                else
                    log_message(LOG_STATUS,
                                "CoolerDash v%s is up to date", current_version);
            }
            json_decref(root);
        }
    }
    else
    {
        log_message(LOG_INFO, "Update check skipped (no network or API unavailable)");
    }

    cc_cleanup_response_buffer(&buf);
    if (headers)
        curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

/**
 * @brief Detect if started by CoolerControl plugin system.
 */
static int is_started_as_plugin(void)
{
    const char *invocation_id = getenv("INVOCATION_ID");
    return (invocation_id && invocation_id[0]) ? 1 : 0;
}

/** @brief Print usage information and exit hints. */
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
    printf("  --split           Force split mode (CPU/GPU columns without bars)\n");
    printf("  --circle          Force circle mode (alternating sensor display)\n");
    printf("  --hardware-report Collect a sanitized hardware report and exit\n");
    printf("  --test-lcd        With --hardware-report: confirm and run an LCD test\n\n");
    printf("ADVANCED REPORT OPTIONS:\n");
    printf("  --output-dir DIR  Report destination (default: invoking user's home)\n");
    printf("  --device UID      Limit report/test to one LCD (default: report all)\n\n");
    printf("DISPLAY MODES:\n");
    printf(
        "  dual              Shows two sensor values with temperature bars\n");
    printf("  split             Shows CPU and GPU in two bar-free columns\n");
    printf("  circle            Alternating mode using the configured interval\n");
    printf("                    Configure via config.json [display] "
           "mode=dual|split|circle or CLI flags\n\n");
    printf("EXAMPLES:\n");
    printf("  sudo systemctl restart coolercontrold     # Restart CoolerControl "
           "(reloads plugin)\n");
    printf("  %s                                # Standalone start with configured "
           "display mode\n",
           program_name);
    printf("  %s --circle                       # Standalone with circle mode "
           "(alternating display)\n",
           program_name);
    printf("  %s --dual --verbose               # Force dual mode with detailed "
           "logging\n",
           program_name);
    printf("  %s --split                        # Two-column CPU/GPU display\n",
           program_name);
    printf("  %s /custom/config.json            # Start with custom "
           "configuration\n\n",
           program_name);
    printf("  %s --hardware-report              # Write JSON + Markdown report to home\n",
           program_name);
    printf("  %s --hardware-report --test-lcd   # Report plus confirmed LCD test\n\n",
           program_name);
    printf("FILES:\n");
    printf("  /usr/libexec/coolerdash/coolerdash            # Main executable\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/         # Plugin data directory\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/config.json # Configuration "
           "file\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/index.html # Web UI settings\n");
    printf("  /etc/coolercontrol/plugins/coolerdash/manifest.toml # Plugin manifest\n");
    printf("  /var/lib/coolercontrol/plugins/coolerdash/.coolerdash.lock # Instance lock\n");
    printf("  journalctl -u coolercontrold.service      # View plugin logs\n\n");
    printf("PLUGIN MODE:\n");
    printf("  - Managed by CoolerControl (coolercontrold.service)\n");
    printf("  - Runs as CoolerControl plugin user (isolated environment)\n");
    printf("  - Communicates via CoolerControl's HTTP API (no direct device "
           "access)\n");
    printf("  - Automatically started/stopped with CoolerControl\n");
    printf("Project repository: https://github.com/damachine/coolerdash\n");
    printf("====================================================================="
           "===========\n");
}

/** @brief Log display dimensions and refresh interval. */
static void show_system_diagnostics(const Config *config, int api_width,
                                    int api_height)
{
    if (!config)
        return;
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

static int acquire_single_instance(const char *runtime_dir)
{
    if (!runtime_dir || runtime_dir[0] == '\0')
        return 0;

    int written = snprintf(s_instance_lock_path, sizeof(s_instance_lock_path),
                           "%s/%s", runtime_dir, COOLERDASH_LOCK_NAME);
    if (written < 0 || (size_t)written >= sizeof(s_instance_lock_path))
    {
        log_message(LOG_ERROR, "Instance lock path is too long");
        return 0;
    }

    int fd = open(s_instance_lock_path, O_RDWR | O_CREAT, 0600);
    if (fd == -1)
    {
        log_message(LOG_ERROR, "Failed to open %s: %s",
                    s_instance_lock_path, strerror(errno));
        return 0;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
    {
        log_message(LOG_ERROR, "Failed to protect %s: %s",
                    s_instance_lock_path, strerror(errno));
        close(fd);
        return 0;
    }

    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
    };
    if (fcntl(fd, F_SETLK, &lock) != 0)
    {
        if (fcntl(fd, F_GETLK, &lock) == 0 && lock.l_pid > 1)
            log_message(LOG_ERROR,
                        "CoolerDash is already running (PID %ld)",
                        (long)lock.l_pid);
        else
            log_message(LOG_ERROR, "CoolerDash is already running");
        close(fd);
        return 0;
    }

    s_instance_fd = fd;
    return 1;
}

static void release_single_instance(void)
{
    if (s_instance_fd >= 0)
    {
        close(s_instance_fd);
        s_instance_fd = -1;
    }
}

/** @brief Async-signal-safe shutdown handler. */
static void handle_shutdown_signal(int signum)
{
    static const char term_msg[] =
        "Received SIGTERM - initiating graceful shutdown\n";
    static const char int_msg[] =
        "Received SIGINT - initiating graceful shutdown\n";
    static const char quit_msg[] =
        "Received SIGQUIT - initiating graceful shutdown\n";
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
    case SIGQUIT:
        msg = quit_msg;
        msg_len = sizeof(quit_msg) - 1;
        break;
    default:
        msg = unknown_msg;
        msg_len = sizeof(unknown_msg) - 1;
        break;
    }

    ssize_t written = write(STDERR_FILENO, msg, msg_len);
    (void)written;
    running = 0;
}

/** @brief SIGHUP handler — sets reload flag. */
static void handle_reload_signal(int signum)
{
    (void)signum;
    static const char msg[] = "Received SIGHUP - scheduling config reload\n";
    ssize_t written = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)written;
    reload_config = 1;
}

/** @brief Install signal handlers for SIGTERM/SIGINT/SIGQUIT/SIGHUP. */
static void setup_enhanced_signal_handlers(void)
{
    struct sigaction sa;
    sigset_t signal_mask;

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

    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGTERM);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGQUIT);
    sigaddset(&signal_mask, SIGHUP);

    int rc = pthread_sigmask(SIG_UNBLOCK, &signal_mask, NULL);
    if (rc != 0)
        log_message(LOG_ERROR, "Failed to unblock daemon signals: %s",
                    strerror(rc));

    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0)
        log_message(LOG_ERROR, "Failed to block SIGPIPE: %s", strerror(rc));
}

/** @brief Re-read config.json on SIGHUP; re-init session and device cache. */
static void reload_daemon_config(Config *config)
{
    log_message(LOG_STATUS, "SIGHUP received — reloading configuration...");

    cleanup_sensor_curl_handle();
    reset_coolercontrol_session();
    reset_device_cache();
    reset_display_state();

    load_plugin_config(config, s_config_path);

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

    update_config_from_device(config);

    log_message(LOG_STATUS, "Configuration reloaded successfully");
}

/** @brief Main daemon loop: renders display on interval, handles SIGHUP. */
static int run_daemon(Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid configuration provided to daemon");
        return -1;
    }

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
        if (reload_config)
        {
            reload_config = 0;
            reload_daemon_config(config);

            interval_sec = (long)config->display_refresh_interval;
            interval_nsec = (long)((config->display_refresh_interval -
                                    interval_sec) *
                                   1000000000);
            interval.tv_sec = interval_sec;
            interval.tv_nsec = interval_nsec;
        }

        next_time.tv_sec += interval.tv_sec;
        next_time.tv_nsec += interval.tv_nsec;
        if (next_time.tv_nsec >= 1000000000L)
        {
            next_time.tv_sec++;
            next_time.tv_nsec -= 1000000000L;
        }

        draw_display_image(config);

        int sleep_result =
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
        if (sleep_result != 0 && sleep_result != EINTR)
        {
            log_message(LOG_WARNING, "Sleep interrupted: %s", strerror(sleep_result));
        }
    }

    return 0;
}

/** @brief Parse CLI arguments. */
static void parse_arguments(int argc, char **argv, CliOptions *options)
{
    memset(options, 0, sizeof(*options));
    options->config_path =
        "/etc/coolercontrol/plugins/coolerdash/config.json";

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
            cc_safe_strcpy(options->display_mode_override,
                           sizeof(options->display_mode_override), "dual");
        }
        else if (strcmp(argv[i], "--split") == 0)
        {
            cc_safe_strcpy(options->display_mode_override,
                           sizeof(options->display_mode_override), "split");
        }
        else if (strcmp(argv[i], "--circle") == 0)
        {
            cc_safe_strcpy(options->display_mode_override,
                           sizeof(options->display_mode_override), "circle");
        }
        else if (strcmp(argv[i], "--hardware-report") == 0)
        {
            options->hardware_report = 1;
        }
        else if (strcmp(argv[i], "--test-lcd") == 0)
        {
            options->report.test_lcd = 1;
        }
        else if (strcmp(argv[i], "--output-dir") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "Error: --output-dir requires a directory\n");
                exit(EXIT_FAILURE);
            }
            options->report.output_dir = argv[i];
        }
        else if (strcmp(argv[i], "--device") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "Error: --device requires a UID\n");
                exit(EXIT_FAILURE);
            }
            options->report.device_uid = argv[i];
        }
        else if (argv[i][0] != '-')
        {
            options->config_path = argv[i];
        }
        else
        {
            fprintf(stderr,
                    "Error: Unknown option '%s'. Use --help for usage information.\n",
                    argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (!options->hardware_report &&
        (options->report.test_lcd || options->report.output_dir ||
         options->report.device_uid))
    {
        fprintf(stderr,
                "Error: --test-lcd, --output-dir, and --device require "
                "--hardware-report\n");
        exit(EXIT_FAILURE);
    }
}

/** @brief Check plugin dir is writable. */
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

/** @brief Load config.json and check dir permissions. */
static int initialize_config_and_instance(const char *config_path,
                                          Config *config)
{
    int json_loaded = load_plugin_config(config, config_path);

    if (!json_loaded)
    {
        log_message(LOG_INFO, "Using hardcoded defaults (no config.json found)");
    }

    int is_plugin_mode = is_started_as_plugin();
    log_message(LOG_INFO, "Running mode: %s",
                is_plugin_mode ? "CoolerControl plugin" : "standalone");

    if (!verify_plugin_dir_permissions(config->paths_images))
    {
        log_message(LOG_ERROR, "Failed to verify plugin directory permissions");
        return 0;
    }

    return 1;
}

/** @brief Init CC session and device cache. */
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

/** @brief Fetch device info, validate sensors, log system state. */
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

/** @brief Cleanup CURL handles and log shutdown. */
static void perform_cleanup(const Config *config)
{
    (void)config;
    log_message(LOG_INFO, "Daemon shutdown initiated");
    cleanup_coolercontrol_session();
    cleanup_sensor_curl_handle();
    running = 0;
    log_message(LOG_INFO, "CoolerDash shutdown complete");
}

/** @brief Daemon entry point. */
int main(int argc, char **argv)
{
    CliOptions cli;
    parse_arguments(argc, argv, &cli);

    s_config_path = cli.config_path;
    cc_safe_strcpy(s_display_mode_override, sizeof(s_display_mode_override),
                   cli.display_mode_override);

    log_message(LOG_STATUS, "CoolerDash v%s starting up...",
                read_version_from_file());

    Config config = {0};
    log_message(LOG_STATUS, "Loading configuration...");

    if (cli.hardware_report)
    {
        int loaded =
            load_plugin_config_read_only(&config, cli.config_path);
        if (!loaded)
            log_message(LOG_INFO,
                        "Using hardcoded defaults (no config.json found)");
        int success = run_hardware_report(
            &config, &cli.report, read_version_from_file());
        memset(config.access_token, 0, sizeof(config.access_token));
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    setup_enhanced_signal_handlers();
    if (!initialize_config_and_instance(cli.config_path, &config))
        return EXIT_FAILURE;

    if (!acquire_single_instance(config.paths_images))
        return EXIT_FAILURE;

    // Apply CLI display mode override if provided
    if (s_display_mode_override[0] != '\0')
    {
        cc_safe_strcpy(config.display_mode, sizeof(config.display_mode),
                       s_display_mode_override);
        log_message(LOG_INFO, "Display mode overridden by CLI: %s",
                    config.display_mode);
    }

    g_config_ptr = &config;

    log_message(LOG_STATUS, "Initializing CoolerControl session...");
    if (initialize_coolercontrol_services(&config) != 0)
    {
        release_single_instance();
        return EXIT_FAILURE;
    }

    log_message(LOG_STATUS, "CoolerDash initializing device cache...\n");
    initialize_device_info(&config);

    // Register shutdown image with CoolerControl at startup.
    // CC stores it internally and applies it automatically when CC shuts down.
    {
        const char *shutdown_image_path = resolve_shutdown_image_path(&config);
        if (shutdown_image_path)
        {
            char shutdown_uid[CC_UID_SIZE] = {0};
            char shutdown_device_name[CONFIG_MAX_STRING_LEN] = {0};
            int shutdown_w = 0, shutdown_h = 0;
            if (get_cached_lcd_device_data(&config, shutdown_uid, sizeof(shutdown_uid),
                                           shutdown_device_name,
                                           sizeof(shutdown_device_name),
                                           &shutdown_w, &shutdown_h) &&
                shutdown_uid[0] != '\0')
            {
                if (strcmp(shutdown_image_path, DEFAULT_SHUTDOWN_IMAGE_PATH) != 0)
                {
                    log_message(LOG_INFO,
                                "Registering custom shutdown image with CoolerControl: %s",
                                shutdown_image_path);
                }

                register_lcd_shutdown_image_with_cc(&config,
                                                    shutdown_image_path,
                                                    shutdown_uid);
            }
            else
            {
                log_message(LOG_WARNING,
                            "Skipping shutdown image registration because no LCD device UID was cached");
            }
        }
    }

    check_for_update(read_version_from_file());

    // Render initial image immediately so the PNG exists on disk
    // before CC applies saved LCD settings (avoids startup race condition)
    log_message(LOG_INFO, "Rendering initial display image...");
    draw_display_image(&config);

    log_message(LOG_STATUS, "Starting daemon");
    int result = run_daemon(&config);

    perform_cleanup(&config);
    release_single_instance();
    return result;
}
