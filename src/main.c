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
#include <fontconfig/fontconfig.h>
#include <jansson.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <curl/curl.h>
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
#define COOLERDASH_LOCK_NAME ".coolerdash.lock"
#define UPDATE_API_URL \
    "https://api.github.com/repos/damachine/coolerdash/releases/latest"
#define UPDATE_RESPONSE_LIMIT (64U * 1024U)
#define UPDATE_VERSION_SIZE 64
#define UPDATE_PATH_SIZE 4096
#define UPDATE_RETRY_ATTEMPTS 4
#define UPDATE_RETRY_DELAY_SECONDS 5
#define UPDATE_PROXY_PORT 11989
#ifndef UPDATE_STATUS_PATH
#define UPDATE_STATUS_PATH \
    DEFAULT_COOLERDASH_PLUGIN_DIR "/update-status.json"
#endif

#ifndef COOLERDASH_VERSION
#define COOLERDASH_VERSION "unknown"
#endif

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_config = 0;

static const char *s_config_path = NULL;
static char s_display_mode_override[16] = {0};
static int s_instance_fd = -1;
static char s_instance_lock_path[CONFIG_MAX_PATH_LEN] = {0};
static pthread_t s_status_server_thread;
static int s_status_server_started = 0;

typedef struct CliOptions
{
    const char *config_path;
    char display_mode_override[16];
    int hardware_report;
    HardwareReportOptions report;
} CliOptions;

typedef struct
{
    char *data;
    size_t size;
} UpdateResponse;

typedef struct
{
    char device_uid[128];
    char firmware_version[CC_FIRMWARE_SIZE];
    int screen_width;
    int screen_height;
} DeviceInfoSnapshot;

static pthread_mutex_t s_device_info_mutex = PTHREAD_MUTEX_INITIALIZER;
static DeviceInfoSnapshot s_device_info = {0};

int verbose_logging = 0;

const Config *g_config_ptr = NULL;

static int parse_version(const char *version, unsigned long parts[],
                         size_t *part_count)
{
    if (!version || !parts || !part_count)
        return 0;

    if (*version == 'v' || *version == 'V')
        version++;
    if (*version == '\0')
        return 0;

    size_t count = 0;
    while (*version != '\0')
    {
        if (count >= 8 || *version < '0' || *version > '9')
            return 0;

        errno = 0;
        char *end = NULL;
        unsigned long value = strtoul(version, &end, 10);
        if (errno == ERANGE || end == version)
            return 0;

        parts[count++] = value;
        if (*end == '\0')
            break;
        if (*end != '.' || end[1] == '\0')
            return 0;
        version = end + 1;
    }

    *part_count = count;
    return count > 0;
}

static int update_compare_versions(const char *left, const char *right,
                                   int *result)
{
    unsigned long left_parts[8] = {0};
    unsigned long right_parts[8] = {0};
    size_t left_count = 0;
    size_t right_count = 0;

    if (!result || !parse_version(left, left_parts, &left_count) ||
        !parse_version(right, right_parts, &right_count))
        return 0;

    size_t count = left_count > right_count ? left_count : right_count;
    *result = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (left_parts[i] == right_parts[i])
            continue;
        *result = left_parts[i] < right_parts[i] ? -1 : 1;
        break;
    }
    return 1;
}

static int update_parse_release(const char *json, char *version,
                                size_t version_size)
{
    if (!json || !version || version_size == 0)
        return 0;

    json_error_t error;
    json_t *root = json_loads(json, JSON_REJECT_DUPLICATES, &error);
    if (!root)
        return 0;

    const json_t *tag = json_object_get(root, "tag_name");
    const char *tag_value = json_string_value(tag);
    int comparison = 0;
    int valid = tag_value && strlen(tag_value) < version_size &&
                update_compare_versions(tag_value, tag_value, &comparison);
    if (valid)
        memcpy(version, tag_value, strlen(tag_value) + 1);

    json_decref(root);
    return valid;
}

static size_t update_write_callback(char *contents, size_t size, size_t nmemb,
                                    void *userdata)
{
    UpdateResponse *response = userdata;
    if (!response || (size != 0 && nmemb > SIZE_MAX / size))
        return 0;

    size_t bytes = size * nmemb;
    if (bytes > UPDATE_RESPONSE_LIMIT - response->size - 1)
        return 0;

    char *new_data = realloc(response->data, response->size + bytes + 1);
    if (!new_data)
        return 0;

    response->data = new_data;
    memcpy(response->data + response->size, contents, bytes);
    response->size += bytes;
    response->data[response->size] = '\0';
    return bytes;
}

static int update_error_is_temporary(CURLcode status)
{
    return status == CURLE_COULDNT_RESOLVE_PROXY ||
           status == CURLE_COULDNT_RESOLVE_HOST ||
           status == CURLE_COULDNT_CONNECT ||
           status == CURLE_OPERATION_TIMEDOUT || status == CURLE_SEND_ERROR ||
           status == CURLE_RECV_ERROR || status == CURLE_GOT_NOTHING;
}

static void update_retry_delay(void)
{
    struct timespec delay = {.tv_sec = UPDATE_RETRY_DELAY_SECONDS,
                             .tv_nsec = 0};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR)
    {
    }
}

static int fetch_latest_version(const char *current_version, char *latest,
                                size_t latest_size)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return 0;

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        curl_global_cleanup();
        return 0;
    }

    UpdateResponse response = {0};
    char user_agent[96];
    snprintf(user_agent, sizeof(user_agent), "CoolerDash/%s", current_version);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    if (!headers)
    {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
    }
    struct curl_slist *new_headers =
        curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    if (!new_headers)
    {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
    }
    headers = new_headers;

    curl_easy_setopt(curl, CURLOPT_URL, UPDATE_API_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, update_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode status = CURLE_OK;
    long http_status = 0;
    int success = 0;
    for (unsigned int attempt = 0; attempt < UPDATE_RETRY_ATTEMPTS; attempt++)
    {
        free(response.data);
        response.data = NULL;
        response.size = 0;
        latest[0] = '\0';
        http_status = 0;

        status = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
        success = status == CURLE_OK && http_status == 200 && response.data &&
                  update_parse_release(response.data, latest, latest_size);
        if (success || !update_error_is_temporary(status) ||
            attempt + 1 == UPDATE_RETRY_ATTEMPTS)
            break;
        update_retry_delay();
    }
    if (!success)
    {
        if (status != CURLE_OK)
            fprintf(stderr, "CoolerDash update check failed: %s\n",
                    curl_easy_strerror(status));
        else
            fprintf(stderr,
                    "CoolerDash update check failed: GitHub returned HTTP %ld\n",
                    http_status);
    }

    free(response.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return success;
}

static int write_all(int fd, const char *data, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        ssize_t written = write(fd, data + offset, length - offset);
        if (written < 0)
        {
            if (errno == EINTR)
                continue;
            return 0;
        }
        if (written == 0)
            return 0;
        offset += (size_t)written;
    }
    return 1;
}

static const char *distribution_name(const char *id)
{
    if (strcmp(id, "arch") == 0)
        return "Arch Linux";
    if (strcmp(id, "gentoo") == 0)
        return "Gentoo";
    if (strcmp(id, "ubuntu") == 0)
        return "Ubuntu";
    if (strcmp(id, "debian") == 0)
        return "Debian";
    if (strcmp(id, "fedora") == 0)
        return "Fedora";
    if (strcmp(id, "centos") == 0)
        return "CentOS";
    if (strcmp(id, "rhel") == 0)
        return "RHEL";
    if (strncmp(id, "opensuse", 8) == 0)
        return "openSUSE";
    return id[0] != '\0' ? id : "Linux";
}

static void detect_platform(char *platform, size_t platform_size)
{
    char id[64] = {0};
    FILE *os_release = fopen("/etc/os-release", "r");
    if (os_release)
    {
        char line[256];
        while (fgets(line, sizeof(line), os_release))
        {
            if (strncmp(line, "ID=", 3) != 0)
                continue;

            char *value = line + 3;
            value[strcspn(value, "\r\n")] = '\0';
            size_t length = strlen(value);
            if (length >= 2 &&
                ((value[0] == '"' && value[length - 1] == '"') ||
                 (value[0] == '\'' && value[length - 1] == '\'')))
            {
                value[length - 1] = '\0';
                value++;
            }
            size_t valid_length = strspn(
                value,
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-");
            if (value[0] != '\0' && value[valid_length] == '\0' &&
                strlen(value) < sizeof(id))
                memcpy(id, value, strlen(value) + 1);
            break;
        }
        fclose(os_release);
    }

    const char *init = NULL;
    if (access("/run/systemd/system", F_OK) == 0)
        init = "systemd";
    else if (access("/run/openrc", F_OK) == 0)
        init = "OpenRC";

    const char *distribution = distribution_name(id);
    if (init)
        snprintf(platform, platform_size, "%s (%s)", distribution, init);
    else
        snprintf(platform, platform_size, "%s", distribution);
}

static void write_update_status(const char *latest)
{
    char platform[128];
    detect_platform(platform, sizeof(platform));

    char payload[384];
    int payload_length = snprintf(
        payload, sizeof(payload),
        "{\"installed\":\"%s\",\"latest\":\"%s\",\"platform\":\"%s\"}\n",
        COOLERDASH_VERSION, latest, platform);
    if (payload_length < 0 || (size_t)payload_length >= sizeof(payload))
        return;

    char temporary[UPDATE_PATH_SIZE];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.%ld",
                 UPDATE_STATUS_PATH, (long)getpid()) >= (int)sizeof(temporary))
        return;

    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return;

    int saved = write_all(fd, payload, (size_t)payload_length) && fsync(fd) == 0;
    if (close(fd) != 0)
        saved = 0;
    if (!saved || rename(temporary, UPDATE_STATUS_PATH) != 0)
        unlink(temporary);
}

static int send_all(int fd, const char *data, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        ssize_t sent = send(fd, data + offset, length - offset, MSG_NOSIGNAL);
        if (sent < 0)
        {
            if (errno == EINTR)
                continue;
            return 0;
        }
        if (sent == 0)
            return 0;
        offset += (size_t)sent;
    }
    return 1;
}

static void update_device_info_snapshot(const Config *config)
{
    DeviceInfoSnapshot next = {0};
    if (get_cached_lcd_device_data(config, next.device_uid,
                                   sizeof(next.device_uid), NULL, 0,
                                   &next.screen_width, &next.screen_height))
    {
        cc_safe_strcpy(next.firmware_version, sizeof(next.firmware_version),
                       get_cached_lcd_firmware_version(config));
    }

    (void)pthread_mutex_lock(&s_device_info_mutex);
    s_device_info = next;
    (void)pthread_mutex_unlock(&s_device_info_mutex);
}

static size_t write_device_info_json(char *body, size_t body_size)
{
    DeviceInfoSnapshot snapshot;
    (void)pthread_mutex_lock(&s_device_info_mutex);
    snapshot = s_device_info;
    (void)pthread_mutex_unlock(&s_device_info_mutex);

    json_t *root = json_pack(
        "{s:s,s:s,s:i,s:i}", "device_uid", snapshot.device_uid,
        "firmware_version", snapshot.firmware_version, "screen_width",
        snapshot.screen_width, "screen_height", snapshot.screen_height);
    if (!root)
        return 0;

    char *payload = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    if (!payload)
        return 0;

    int length = snprintf(body, body_size, "%s\n", payload);
    free(payload);
    if (length < 0 || (size_t)length >= body_size)
        return 0;
    return (size_t)length;
}

static char *get_font_families_json(void)
{
    FcPattern *pattern = NULL;
    FcObjectSet *objects = NULL;
    FcFontSet *fonts = NULL;
    FcStrSet *families = NULL;
    FcStrList *family_list = NULL;
    json_t *array = NULL;
    json_t *root = NULL;
    char *payload = NULL;

    pattern = FcPatternCreate();
    objects = FcObjectSetBuild(FC_FAMILY, NULL);
    if (!pattern || !objects)
        goto cleanup;

    fonts = FcFontList(NULL, pattern, objects);
    families = FcStrSetCreate();
    if (!fonts || !families)
        goto cleanup;

    for (int i = 0; i < fonts->nfont; i++)
    {
        for (int index = 0;; index++)
        {
            FcChar8 *family = NULL;
            if (FcPatternGetString(fonts->fonts[i], FC_FAMILY, index, &family) !=
                FcResultMatch)
                break;
            if (family[0] != '\0' && !FcStrSetAdd(families, family))
                goto cleanup;
        }
    }

    family_list = FcStrListCreate(families);
    array = json_array();
    if (!family_list || !array)
        goto cleanup;

    const FcChar8 *family;
    while ((family = FcStrListNext(family_list)) != NULL)
    {
        json_t *name = json_string((const char *)family);
        if (!name)
            goto cleanup;
        int append_result = json_array_append(array, name);
        json_decref(name);
        if (append_result != 0)
            goto cleanup;
    }

    root = json_object();
    if (!root || json_object_set(root, "fonts", array) != 0)
        goto cleanup;
    payload = json_dumps(root, JSON_COMPACT);

cleanup:
    json_decref(root);
    json_decref(array);
    if (family_list)
        FcStrListDone(family_list);
    if (families)
        FcStrSetDestroy(families);
    if (fonts)
        FcFontSetDestroy(fonts);
    if (objects)
        FcObjectSetDestroy(objects);
    if (pattern)
        FcPatternDestroy(pattern);
    return payload;
}

static void serve_plugin_data(int client_fd)
{
    char request[CONFIG_MAX_PATH_LEN * 3 + 128] = {0};
    size_t request_length = 0;
    while (!strchr(request, '\n'))
    {
        ssize_t received = recv(client_fd, request + request_length,
                                sizeof(request) - 1 - request_length, 0);
        if (received <= 0)
            return;
        request_length += (size_t)received;
        request[request_length] = '\0';
        if (request_length == sizeof(request) - 1)
            return;
    }

    const char *status = "200 OK";
    const char *content_type = "application/json";
    char body[384] = "{}\n";
    size_t body_length = strlen(body);
    char *dynamic_body = NULL;

    if (strncmp(request, "GET /status HTTP/", 17) == 0)
    {
        int status_fd = open(UPDATE_STATUS_PATH, O_RDONLY);
        if (status_fd >= 0)
        {
            ssize_t read_length = read(status_fd, body, sizeof(body) - 1);
            close(status_fd);
            if (read_length > 0)
            {
                body[read_length] = '\0';
                body_length = (size_t)read_length;
            }
        }
    }
    else if (strncmp(request, "GET /device-info HTTP/", 22) == 0)
    {
        size_t json_length = write_device_info_json(body, sizeof(body));
        if (json_length > 0)
            body_length = json_length;
    }
    else if (strncmp(request, "GET /background-preview?path=", 29) == 0)
    {
        const char *encoded = request + 29;
        const char *end = strchr(encoded, ' ');
        int decoded_length = 0;
        char *path = end ? curl_easy_unescape(NULL, encoded, (int)(end - encoded),
                                             &decoded_length) : NULL;
        char *image = NULL;
        if (path && decoded_length > 0 && decoded_length < CONFIG_MAX_PATH_LEN &&
            strlen(path) == (size_t)decoded_length)
            image = image_file_preview_data_uri(path);
        curl_free(path);
        if (image)
        {
            json_t *root = json_pack("{s:s}", "image", image);
            if (root)
            {
                dynamic_body = json_dumps(root, JSON_COMPACT);
                json_decref(root);
            }
            free(image);
        }
        if (dynamic_body)
            body_length = strlen(dynamic_body);
        else
            status = "422 Unprocessable Content";
    }
    else if (strncmp(request, "GET /fonts HTTP/", 16) == 0)
    {
        dynamic_body = get_font_families_json();
        if (dynamic_body)
            body_length = strlen(dynamic_body);
        else
            status = "500 Internal Server Error";
    }
    else
    {
        status = "404 Not Found";
    }

    char header[256];
    int header_length = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
        status, content_type, body_length);
    if (header_length <= 0 || (size_t)header_length >= sizeof(header))
    {
        free(dynamic_body);
        return;
    }
    (void)send_all(client_fd, header, (size_t)header_length);
    (void)send_all(client_fd, dynamic_body ? dynamic_body : body, body_length);
    free(dynamic_body);
}

static void *run_plugin_data_server(void *unused)
{
    (void)unused;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        log_message(LOG_WARNING, "Could not create plugin data server: %s",
                    strerror(errno));
        return NULL;
    }

    int reuse = 1;
    (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(UPDATE_PROXY_PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(server_fd, 4) != 0)
    {
        log_message(LOG_WARNING,
                    "Could not listen for plugin data on 127.0.0.1:%d: %s",
                    UPDATE_PROXY_PORT, strerror(errno));
        close(server_fd);
        return NULL;
    }

    log_message(LOG_INFO, "Plugin data available on 127.0.0.1:%d",
                UPDATE_PROXY_PORT);
    while (running)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
        int ready = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0)
            continue;

        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        struct timeval client_timeout = {.tv_sec = 2, .tv_usec = 0};
        (void)setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &client_timeout,
                         sizeof(client_timeout));
        (void)setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &client_timeout,
                         sizeof(client_timeout));
        serve_plugin_data(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return NULL;
}

static void start_status_server(void)
{
    int result = pthread_create(&s_status_server_thread, NULL,
                                run_plugin_data_server, NULL);
    if (result != 0)
    {
        log_message(LOG_WARNING, "Could not start plugin data server: %s",
                    strerror(result));
        return;
    }
    s_status_server_started = 1;
}

static void stop_status_server(void)
{
    if (!s_status_server_started)
        return;
    (void)pthread_join(s_status_server_thread, NULL);
    s_status_server_started = 0;
}

static void refresh_update_status_async(void)
{
    pid_t launcher = fork();
    if (launcher < 0)
    {
        log_message(LOG_INFO, "Could not start background update check: %s",
                    strerror(errno));
        return;
    }
    if (launcher == 0)
    {
        pid_t worker = fork();
        if (worker == 0)
        {
            char latest[UPDATE_VERSION_SIZE] = {0};
            (void)fetch_latest_version(COOLERDASH_VERSION, latest,
                                       sizeof(latest));
            write_update_status(latest);
            _exit(EXIT_SUCCESS);
        }
        _exit(worker < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
    }

    int status = 0;
    while (waitpid(launcher, &status, 0) < 0 && errno == EINTR)
    {
    }
}

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

    const char *version = COOLERDASH_VERSION;

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
    printf("  /var/lib/coolercontrol/plugins/coolerdash/         # Plugin data directory\n");
    printf("  /var/lib/coolercontrol/plugins/coolerdash/config.json # Configuration "
           "file\n");
    printf("  /var/lib/coolercontrol/plugins/coolerdash/ui/index.html # Web UI settings\n");
    printf("  /var/lib/coolercontrol/plugins/coolerdash/manifest.toml # Plugin manifest\n");
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
    update_device_info_snapshot(config);

    log_message(LOG_STATUS, "Configuration reloaded successfully");
}

/** @brief Select the shorter of the configured refresh and GIF frame delay. */
static void set_render_interval(const Config *config, struct timespec *interval)
{
    double seconds = config->display_refresh_interval;
    const int animation_delay_ms = display_background_animation_delay_ms();

    if (animation_delay_ms > 0 && animation_delay_ms / 1000.0 < seconds)
        seconds = animation_delay_ms / 1000.0;

    interval->tv_sec = (time_t)seconds;
    interval->tv_nsec = (long)((seconds - interval->tv_sec) * 1000000000.0);
}

/** @brief Main daemon loop: renders display on interval, handles SIGHUP. */
static int run_daemon(Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid configuration provided to daemon");
        return -1;
    }

    struct timespec interval;
    set_render_interval(config, &interval);

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

            set_render_interval(config, &interval);
        }

        draw_display_image(config);
        set_render_interval(config, &interval);

        next_time.tv_sec += interval.tv_sec;
        next_time.tv_nsec += interval.tv_nsec;
        if (next_time.tv_nsec >= 1000000000L)
        {
            next_time.tv_sec++;
            next_time.tv_nsec -= 1000000000L;
        }

        /* Absolute deadline: a render or a reload longer than one interval leaves it in
           the past, and clock_nanosleep then returns at once. Drop the missed ticks
           rather than replaying them at render speed. */
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
            (next_time.tv_sec < now.tv_sec ||
             (next_time.tv_sec == now.tv_sec && next_time.tv_nsec < now.tv_nsec)))
        {
            next_time = now;
        }

        int sleep_result;
        while ((sleep_result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                               &next_time, NULL)) == EINTR)
        {
            /* The deadline is absolute, so resuming is the same call. Only a shutdown or
               a reload is worth cutting the interval short for. */
            if (!running || reload_config)
                break;
        }
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
    options->config_path = DEFAULT_COOLERDASH_PLUGIN_DIR "/config.json";

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

    if (!display_has_active_sensor_slots(config))
    {
        log_message(LOG_STATUS, "Background-only mode: all sensor slots disabled");
    }
    else if (get_sensor_monitor_data(config, &temp_data))
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
    update_device_info_snapshot(config);
}

/** @brief Cleanup CURL handles and log shutdown. */
static void perform_cleanup(const Config *config)
{
    (void)config;
    log_message(LOG_INFO, "Daemon shutdown initiated");
    stop_status_server();
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
                COOLERDASH_VERSION);

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
            &config, &cli.report, COOLERDASH_VERSION);
        memset(config.access_token, 0, sizeof(config.access_token));
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    setup_enhanced_signal_handlers();
    if (!initialize_config_and_instance(cli.config_path, &config))
        return EXIT_FAILURE;

    if (!acquire_single_instance(config.paths_images))
        return EXIT_FAILURE;

    write_update_status("");
    refresh_update_status_async();

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

    start_status_server();

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
