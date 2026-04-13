/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
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
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <jansson.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "device/config.h"
#include "mods/display.h"
#include "srv/cc_conf.h"
#include "srv/cc_main.h"
#include "srv/cc_sensor.h"

// Security and performance constants
#define DEFAULT_VERSION "unknown"
#define VERSION_BUFFER_SIZE 32
#define CC4_MODE_LOCK "/etc/coolercontrol/plugins/coolerdash/.cc4-mode"
#define GH_UPDATE_URL "https://api.github.com/repos/damachine/coolerdash/releases/latest"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_config = 0;

static const char *s_config_path = NULL;
static char s_display_mode_override[16] = {0};

int verbose_logging = 0;

const Config *g_config_ptr = NULL;

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

/** @brief Send desktop notification for available update; opens browser on click.
 *  @details Runs as root (systemd service), so we must find the active desktop
 *           user, set DBUS_SESSION_BUS_ADDRESS / XDG_RUNTIME_DIR, and drop
 *           privileges before calling notify-send.
 */
static void send_update_notification(const char *current, const char *latest)
{
    if (access("/usr/bin/notify-send", X_OK) != 0)
        return;

    char body[128];
    int n = snprintf(body, sizeof(body), "v%s → %s available", current, latest);
    if (n < 0 || (size_t)n >= sizeof(body))
        return;

    char url[128];
    n = snprintf(url, sizeof(url),
                 "https://github.com/damachine/coolerdash/releases/tag/%s",
                 latest);
    if (n < 0 || (size_t)n >= sizeof(url))
        return;

    /* ── Find active desktop user via /run/user/<uid>/bus ── */
    DIR *rundir = opendir("/run/user");
    if (!rundir)
        return;

    uid_t target_uid = (uid_t)-1;
    struct dirent *ent;
    while ((ent = readdir(rundir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;
        char bus_path[280];
        snprintf(bus_path, sizeof(bus_path),
                 "/run/user/%s/bus", ent->d_name);
        if (access(bus_path, F_OK) == 0)
        {
            target_uid = (uid_t)strtoul(ent->d_name, NULL, 10);
            break;
        }
    }
    closedir(rundir);

    if (target_uid == (uid_t)-1)
        return;

    struct passwd *pw = getpwuid(target_uid);
    if (!pw)
        return;

    pid_t pid = fork();
    if (pid < 0)
        return;

    if (pid == 0)
    {
        /* Child: set D-Bus environment and drop to desktop user */
        char dbus_addr[128];
        snprintf(dbus_addr, sizeof(dbus_addr),
                 "unix:path=/run/user/%u/bus", (unsigned)target_uid);
        setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1);

        char xdg_dir[64];
        snprintf(xdg_dir, sizeof(xdg_dir),
                 "/run/user/%u", (unsigned)target_uid);
        setenv("XDG_RUNTIME_DIR", xdg_dir, 1);
        setenv("HOME", pw->pw_dir, 1);

        /* Detect display server: Wayland socket or X11 fallback */
        char wayland_path[128];
        snprintf(wayland_path, sizeof(wayland_path),
                 "/run/user/%u/wayland-0", (unsigned)target_uid);
        if (access(wayland_path, F_OK) == 0)
        {
            setenv("WAYLAND_DISPLAY", "wayland-0", 1);
        }
        else
        {
            setenv("DISPLAY", ":0", 1);
        }

        /* Drop privileges to desktop user */
        if (setgid(pw->pw_gid) != 0 || setuid(target_uid) != 0)
            _exit(1);

        /* Fork grandchild for notify-send with action pipe */
        int pfd[2];
        if (pipe(pfd) != 0)
            _exit(1);

        pid_t np = fork();
        if (np < 0)
            _exit(1);

        if (np == 0)
        {
            /* Grandchild: exec notify-send, stdout → pipe */
            close(pfd[0]);
            dup2(pfd[1], STDOUT_FILENO);
            close(pfd[1]);
            execlp("notify-send", "notify-send",
                   "--wait",
                   "-t", "10000",
                   "-i", "coolerdash",
                   "-a", "CoolerDash",
                   "-A", "open=Download",
                   "CoolerDash Update", body,
                   (char *)NULL);
            _exit(1);
        }

        /* Child: read action from notify-send stdout */
        close(pfd[1]);
        char action[32] = {0};
        ssize_t rd = read(pfd[0], action, sizeof(action) - 1);
        close(pfd[0]);
        waitpid(np, NULL, 0);

        if (rd > 0)
        {
            action[strcspn(action, "\n")] = '\0';
            if (strcmp(action, "open") == 0 &&
                access("/usr/bin/xdg-open", X_OK) == 0)
            {
                execlp("xdg-open", "xdg-open", url, (char *)NULL);
            }
        }
        _exit(0);
    }

    /* Parent: don't block — reap child asynchronously */
    signal(SIGCHLD, SIG_IGN);
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
                    send_update_notification(current_version, latest);
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
    printf("  --circle          Force circle mode (alternating CPU/GPU every 2.5 "
           "seconds)\n\n");
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

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGPIPE);

    if (pthread_sigmask(SIG_BLOCK, &block_mask, NULL) != 0)
    {
        log_message(LOG_WARNING, "Failed to block unwanted signals");
    }
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

/** @brief Parse CLI args; returns config path. */
static const char *parse_arguments(int argc, char **argv,
                                   char *display_mode_override)
{
    const char *config_path = "/etc/coolercontrol/plugins/coolerdash/config.json";
    display_mode_override[0] = '\0';

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
    char display_mode_override[16] = {0};
    const char *config_path = parse_arguments(argc, argv, display_mode_override);

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

    /* CC4 lock file: disable helper service on next boot when token is set */
    if (config.access_token[0] != '\0')
    {
        FILE *lf = fopen(CC4_MODE_LOCK, "w");
        if (lf)
            fclose(lf);
    }
    else
    {
        unlink(CC4_MODE_LOCK);
    }

    log_message(LOG_STATUS, "CoolerDash initializing device cache...\n");
    initialize_device_info(&config);
    check_for_update(read_version_from_file());

    // Render initial image immediately so the PNG exists on disk
    // before CC applies saved LCD settings (avoids startup race condition)
    log_message(LOG_INFO, "Rendering initial display image...");
    draw_display_image(&config);

    log_message(LOG_STATUS, "Starting daemon");
    int result = run_daemon(&config);

    perform_cleanup(&config);
    return result;
}
