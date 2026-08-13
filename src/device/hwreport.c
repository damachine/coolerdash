/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Read-only hardware report collection and optional LCD test.
 */

#define _POSIX_C_SOURCE 200809L

// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <curl/curl.h>
#include <jansson.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

#include "hwreport.h"

#define REPORT_SCHEMA_VERSION 2
#define REPORT_MAX_DEVICES 32
#define REPORT_MAX_USB_IDS 64
#define REPORT_MAX_SECRETS 96
#define REPORT_COMMAND_LIMIT (1024U * 1024U)
#define REPORT_LOG_LIMIT (64U * 1024U)
#define REPORT_LOG_LINES 200
#define REPORT_HTTP_TIMEOUT 10L
#define REPORT_COMMAND_TIMEOUT_MS 5000
#define REPORT_NZXT_VENDOR_ID 0x1e71U

typedef struct ReportBuffer
{
    char *data;
    size_t size;
    size_t capacity;
} ReportBuffer;

typedef struct ReportDevice
{
    char uid[128];
    char report_id[32];
    char name[256];
    char channel[128];
    int width;
    int height;
} ReportDevice;

typedef struct UsbId
{
    unsigned vendor_id;
    unsigned product_id;
} UsbId;

typedef struct ReportContext
{
    const Config *config;
    const HardwareReportOptions *options;
    json_t *root;
    json_t *warnings;
    ReportDevice devices[REPORT_MAX_DEVICES];
    size_t device_count;
    UsbId usb_ids[REPORT_MAX_USB_IDS];
    size_t usb_id_count;
    char *secrets[REPORT_MAX_SECRETS];
    size_t secret_count;
    char home_dir[HARDWARE_REPORT_PATH_SIZE];
    char hostname[256];
} ReportContext;

static volatile sig_atomic_t lcd_test_interrupted = 0;

static void lcd_test_signal_handler(int signum)
{
    (void)signum;
    lcd_test_interrupted = 1;
}

static int buffer_init(ReportBuffer *buffer, size_t capacity)
{
    if (!buffer || capacity == 0)
        return 0;

    buffer->data = malloc(capacity);
    if (!buffer->data)
        return 0;

    buffer->size = 0;
    buffer->capacity = capacity;
    buffer->data[0] = '\0';
    return 1;
}

static void buffer_free(ReportBuffer *buffer)
{
    if (!buffer)
        return;
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

static int buffer_reserve(ReportBuffer *buffer, size_t required)
{
    if (!buffer || required > REPORT_COMMAND_LIMIT + 1)
        return 0;
    if (required <= buffer->capacity)
        return 1;

    size_t capacity = buffer->capacity;
    while (capacity < required)
    {
        if (capacity >= REPORT_COMMAND_LIMIT / 2)
        {
            capacity = REPORT_COMMAND_LIMIT + 1;
            break;
        }
        capacity *= 2;
    }

    char *resized = realloc(buffer->data, capacity);
    if (!resized)
        return 0;
    buffer->data = resized;
    buffer->capacity = capacity;
    return 1;
}

static int buffer_append(ReportBuffer *buffer, const char *text, size_t length)
{
    if (!buffer || !text || length == 0)
        return 1;
    if (buffer->size + length > REPORT_COMMAND_LIMIT)
        length = REPORT_COMMAND_LIMIT - buffer->size;
    if (length == 0 ||
        !buffer_reserve(buffer, buffer->size + length + 1))
        return 0;

    memcpy(buffer->data + buffer->size, text, length);
    buffer->size += length;
    buffer->data[buffer->size] = '\0';
    return 1;
}

// cppcheck-suppress constParameterCallback
static size_t curl_write_report(void *contents, size_t size, size_t nmemb,
                                void *userdata)
{
    ReportBuffer *buffer = userdata;
    if (size != 0 && nmemb > SIZE_MAX / size)
        return 0;
    size_t total = size * nmemb;
    return buffer_append(buffer, contents, total) ? total : 0;
}

static int string_is_uint(const char *value)
{
    if (!value || value[0] == '\0')
        return 0;
    for (size_t i = 0; value[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)value[i]))
            return 0;
    }
    return 1;
}

static int contains_ci(const char *haystack, const char *needle)
{
    if (!haystack || !needle || needle[0] == '\0')
        return 0;

    const size_t needle_len = strlen(needle);
    for (size_t i = 0; haystack[i] != '\0'; i++)
    {
        size_t j = 0;
        while (j < needle_len && haystack[i + j] != '\0' &&
               tolower((unsigned char)haystack[i + j]) ==
                   tolower((unsigned char)needle[j]))
        {
            j++;
        }
        if (j == needle_len)
            return 1;
    }
    return 0;
}

static void add_warning(ReportContext *context, const char *message)
{
    if (!context || !context->warnings || !message)
        return;
    json_array_append_new(context->warnings, json_string(message));
}

static void add_secret(ReportContext *context, const char *value)
{
    if (!context || !value || strlen(value) < 3 ||
        context->secret_count >= REPORT_MAX_SECRETS)
        return;

    for (size_t i = 0; i < context->secret_count; i++)
    {
        if (strcmp(context->secrets[i], value) == 0)
            return;
    }

    char *copy = strdup(value);
    if (copy)
        context->secrets[context->secret_count++] = copy;
}

static void free_secrets(ReportContext *context)
{
    if (!context)
        return;
    for (size_t i = 0; i < context->secret_count; i++)
        free(context->secrets[i]);
    context->secret_count = 0;
}

static int replace_all(ReportBuffer *output, const char *input,
                       const char *needle, const char *replacement)
{
    if (!output || !input || !needle || needle[0] == '\0' || !replacement)
        return 0;

    const size_t needle_len = strlen(needle);
    const size_t replacement_len = strlen(replacement);
    const char *cursor = input;
    const char *match;

    while ((match = strstr(cursor, needle)) != NULL)
    {
        if (!buffer_append(output, cursor, (size_t)(match - cursor)) ||
            !buffer_append(output, replacement, replacement_len))
            return 0;
        cursor = match + needle_len;
    }
    return buffer_append(output, cursor, strlen(cursor));
}

static int looks_like_ipv4_at(const char *text, size_t *length)
{
    size_t pos = 0;
    for (int part = 0; part < 4; part++)
    {
        int digits = 0;
        int value = 0;
        while (isdigit((unsigned char)text[pos]) && digits < 3)
        {
            value = value * 10 + (text[pos] - '0');
            pos++;
            digits++;
        }
        if (digits == 0 || value > 255)
            return 0;
        if (part < 3)
        {
            if (text[pos] != '.')
                return 0;
            pos++;
        }
    }
    if (isdigit((unsigned char)text[pos]) || text[pos] == '.')
        return 0;
    *length = pos;
    return 1;
}

static size_t ipv6_length_at(const char *text)
{
    size_t length = 0;
    int colon_count = 0;
    while (isxdigit((unsigned char)text[length]) || text[length] == ':')
    {
        if (text[length] == ':')
            colon_count++;
        length++;
    }
    if (colon_count < 2 || length < 2 || length >= INET6_ADDRSTRLEN)
        return 0;

    char candidate[INET6_ADDRSTRLEN];
    memcpy(candidate, text, length);
    candidate[length] = '\0';
    struct in6_addr address;
    return inet_pton(AF_INET6, candidate, &address) == 1 ? length : 0;
}

static char *redact_ip_addresses(const char *input)
{
    ReportBuffer output;
    if (!buffer_init(&output, strlen(input) + 32))
        return NULL;

    size_t i = 0;
    while (input[i] != '\0')
    {
        size_t ip_length = 0;
        if ((i == 0 || !isalnum((unsigned char)input[i - 1])) &&
            looks_like_ipv4_at(input + i, &ip_length))
        {
            if (!buffer_append(&output, "[redacted-ip]", 13))
            {
                buffer_free(&output);
                return NULL;
            }
            i += ip_length;
            continue;
        }
        size_t ipv6_length = ipv6_length_at(input + i);
        if (ipv6_length > 0)
        {
            if (!buffer_append(&output, "[redacted-ip]", 13))
            {
                buffer_free(&output);
                return NULL;
            }
            i += ipv6_length;
            continue;
        }
        if (!buffer_append(&output, input + i, 1))
        {
            buffer_free(&output);
            return NULL;
        }
        i++;
    }
    return output.data;
}

static char *redact_home_paths(const char *input)
{
    ReportBuffer output;
    if (!buffer_init(&output, strlen(input) + 32))
        return NULL;

    size_t i = 0;
    while (input[i] != '\0')
    {
        int home_path = strncmp(input + i, "/home/", 6) == 0;
        int root_path = strncmp(input + i, "/root/", 6) == 0;
        if (home_path || root_path)
        {
            if (!buffer_append(&output, "[redacted-path]", 15))
            {
                buffer_free(&output);
                return NULL;
            }
            i += 6;
            while (input[i] != '\0' &&
                   !isspace((unsigned char)input[i]) &&
                   input[i] != '"' && input[i] != '\'' &&
                   input[i] != ')' && input[i] != ']')
                i++;
            continue;
        }
        if (!buffer_append(&output, input + i, 1))
        {
            buffer_free(&output);
            return NULL;
        }
        i++;
    }
    return output.data;
}

static char *redact_text(const ReportContext *context, const char *input)
{
    if (!input)
        return strdup("");

    char *current = redact_ip_addresses(input);
    if (!current)
        return NULL;
    char *paths_redacted = redact_home_paths(current);
    free(current);
    current = paths_redacted;
    if (!current)
        return NULL;

    for (size_t i = 0; context && i < context->secret_count; i++)
    {
        if (context->secrets[i][0] == '\0')
            continue;

        ReportBuffer replaced;
        if (!buffer_init(&replaced, strlen(current) + 32) ||
            !replace_all(&replaced, current, context->secrets[i], "[redacted]"))
        {
            buffer_free(&replaced);
            free(current);
            return NULL;
        }
        free(current);
        current = replaced.data;
    }
    return current;
}

static int resolve_default_output_dir(char *buffer, size_t buffer_size,
                                      uid_t real_uid, uid_t effective_uid,
                                      const char *sudo_uid)
{
    if (!buffer || buffer_size == 0)
        return 0;

    uid_t uid = real_uid;
    if (effective_uid == 0)
    {
        if (string_is_uint(sudo_uid))
        {
            unsigned long parsed = strtoul(sudo_uid, NULL, 10);
            if (parsed <= (unsigned long)(uid_t)-1)
                uid = (uid_t)parsed;
        }
    }

    const struct passwd *entry = getpwuid(uid);
    if (!entry || !entry->pw_dir || entry->pw_dir[0] != '/')
        return 0;

    int written = snprintf(buffer, buffer_size, "%s", entry->pw_dir);
    return written > 0 && (size_t)written < buffer_size;
}

int hardware_report_default_output_dir(char *buffer, size_t buffer_size)
{
    return resolve_default_output_dir(buffer, buffer_size, getuid(), geteuid(),
                                      getenv("SUDO_UID"));
}

#ifdef HARDWARE_REPORT_TESTING
int hardware_report_default_output_dir_for_test(
    char *buffer, size_t buffer_size, uid_t real_uid, uid_t effective_uid,
    const char *sudo_uid)
{
    return resolve_default_output_dir(buffer, buffer_size, real_uid,
                                      effective_uid, sudo_uid);
}
#endif

static int http_request(const Config *config, const char *method,
                        const char *path, const char *body,
                        const char *content_type, ReportBuffer *response,
                        long *http_code)
{
    if (!config || !method || !path || !response || !http_code ||
        config->access_token[0] == '\0')
        return 0;

    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    char url[1024];
    int written = snprintf(url, sizeof(url), "%s%s",
                           config->daemon_address, path);
    if (written < 0 || (size_t)written >= sizeof(url))
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    char authorization[CONFIG_MAX_TOKEN_LEN + 32];
    written = snprintf(authorization, sizeof(authorization),
                       "Authorization: Bearer %s", config->access_token);
    if (written < 0 || (size_t)written >= sizeof(authorization))
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, authorization);
    headers = curl_slist_append(headers, "Accept: application/json");
    if (content_type)
    {
        char type_header[128];
        written = snprintf(type_header, sizeof(type_header),
                           "Content-Type: %s", content_type);
        if (written > 0 && (size_t)written < sizeof(type_header))
            headers = curl_slist_append(headers, type_header);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_report);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REPORT_HTTP_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (body)
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);

    if (headers)
        curl_slist_free_all(headers);
    memset(authorization, 0, sizeof(authorization));
    curl_easy_cleanup(curl);
    return result == CURLE_OK;
}

static json_t *get_json_endpoint(ReportContext *context, const char *path,
                                 const char *warning)
{
    ReportBuffer response;
    if (!buffer_init(&response, 4096))
        return NULL;

    long code = 0;
    int ok = http_request(context->config, "GET", path, NULL, NULL,
                          &response, &code);
    if (!ok || code != 200)
    {
        add_warning(context, warning);
        buffer_free(&response);
        return NULL;
    }

    json_error_t error;
    json_t *root = json_loadb(response.data, response.size, 0, &error);
    buffer_free(&response);
    if (!root)
    {
        add_warning(context, warning);
        return NULL;
    }
    return root;
}

static int run_command(char *const argv[], ReportBuffer *output,
                       int *exit_status)
{
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return 0;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        setenv("LANG", "C", 1);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0)
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    int elapsed_ms = 0;
    int child_done = 0;
    int status = 0;
    while (!child_done && elapsed_ms < REPORT_COMMAND_TIMEOUT_MS)
    {
        struct pollfd descriptor = {
            .fd = pipefd[0], .events = POLLIN | POLLHUP, .revents = 0};
        (void)poll(&descriptor, 1, 100);
        elapsed_ms += 100;

        char chunk[4096];
        ssize_t count;
        while ((count = read(pipefd[0], chunk, sizeof(chunk))) > 0)
            buffer_append(output, chunk, (size_t)count);

        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid)
            child_done = 1;
    }

    if (!child_done)
    {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }

    char chunk[4096];
    ssize_t count;
    while ((count = read(pipefd[0], chunk, sizeof(chunk))) > 0)
        buffer_append(output, chunk, (size_t)count);
    close(pipefd[0]);

    if (exit_status)
    {
        if (child_done && WIFEXITED(status))
            *exit_status = WEXITSTATUS(status);
        else
            *exit_status = 124;
    }
    return 1;
}

static const char *json_string_or_null(const json_t *object, const char *key)
{
    const json_t *value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : NULL;
}

static void copy_string_field(json_t *destination, const char *destination_key,
                              const json_t *source, const char *source_key)
{
    const char *value = json_string_or_null(source, source_key);
    if (value)
        json_object_set_new(destination, destination_key, json_string(value));
}

static void copy_integer_field(json_t *destination, const char *destination_key,
                               const json_t *source, const char *source_key)
{
    const json_t *value = json_object_get(source, source_key);
    if (json_is_integer(value))
        json_object_set_new(destination, destination_key,
                            json_integer(json_integer_value(value)));
}

static void copy_number_field(json_t *destination, const char *destination_key,
                              const json_t *source, const char *source_key)
{
    const json_t *value = json_object_get(source, source_key);
    if (json_is_number(value))
        json_object_set_new(destination, destination_key,
                            json_real(json_number_value(value)));
}

static void copy_boolean_field(json_t *destination, const char *destination_key,
                               const json_t *source, const char *source_key)
{
    const json_t *value = json_object_get(source, source_key);
    if (json_is_boolean(value))
        json_object_set_new(destination, destination_key,
                            json_boolean(json_is_true(value)));
}

static void copy_string_array_field(json_t *destination,
                                    const char *destination_key,
                                    const json_t *source,
                                    const char *source_key)
{
    const json_t *values = json_object_get(source, source_key);
    if (!json_is_array(values))
        return;

    json_t *clean = json_array();
    size_t index;
    json_t *value;
    json_array_foreach(values, index, value)
    {
        if (json_is_string(value))
            json_array_append_new(clean,
                                  json_string(json_string_value(value)));
    }
    json_object_set_new(destination, destination_key, clean);
}

static void trim_line(char *value)
{
    if (!value)
        return;
    value[strcspn(value, "\r\n")] = '\0';
    size_t length = strlen(value);
    while (length > 0 && isspace((unsigned char)value[length - 1]))
        value[--length] = '\0';
}

static int read_small_file(const char *path, char *buffer, size_t buffer_size)
{
    if (!path || !buffer || buffer_size == 0)
        return 0;
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    int ok = fgets(buffer, (int)buffer_size, file) != NULL;
    fclose(file);
    if (ok)
        trim_line(buffer);
    return ok;
}

static json_t *collect_system_info(ReportContext *context)
{
    json_t *system = json_object();
    if (!system)
        return NULL;

    struct utsname data;
    if (uname(&data) == 0)
    {
        json_object_set_new(system, "kernel", json_string(data.release));
        json_object_set_new(system, "architecture", json_string(data.machine));
        snprintf(context->hostname, sizeof(context->hostname), "%s", data.nodename);
        add_secret(context, context->hostname);
    }
    else
    {
        add_warning(context, "Unable to read kernel information");
    }

    FILE *os_release = fopen("/etc/os-release", "r");
    if (!os_release)
    {
        add_warning(context, "Unable to read /etc/os-release");
        return system;
    }

    char line[1024];
    while (fgets(line, sizeof(line), os_release))
    {
        char *separator = strchr(line, '=');
        if (!separator)
            continue;
        *separator = '\0';
        char *value = separator + 1;
        trim_line(value);
        size_t length = strlen(value);
        if (length >= 2 &&
            ((value[0] == '"' && value[length - 1] == '"') ||
             (value[0] == '\'' && value[length - 1] == '\'')))
        {
            value[length - 1] = '\0';
            value++;
        }

        if (strcmp(line, "ID") == 0)
            json_object_set_new(system, "distribution_id", json_string(value));
        else if (strcmp(line, "VERSION_ID") == 0)
            json_object_set_new(system, "distribution_version",
                                json_string(value));
        else if (strcmp(line, "PRETTY_NAME") == 0)
            json_object_set_new(system, "distribution", json_string(value));
    }
    fclose(os_release);
    return system;
}

static json_t *sanitize_health(const json_t *health)
{
    json_t *clean = json_object();
    if (!clean)
        return NULL;

    copy_string_field(clean, "status", health, "status");
    copy_string_field(clean, "description", health, "description");

    const json_t *details = json_object_get(health, "details");
    if (json_is_object(details))
    {
        json_t *clean_details = json_object();
        copy_string_field(clean_details, "version", details, "version");
        copy_string_field(clean_details, "uptime", details, "uptime");
        copy_integer_field(clean_details, "warnings", details, "warnings");
        copy_integer_field(clean_details, "errors", details, "errors");
        copy_boolean_field(clean_details, "liquidctl_connected", details,
                           "liquidctl_connected");
        json_object_set_new(clean, "details", clean_details);
    }
    return clean;
}

static json_t *sanitize_lcd_info(const json_t *lcd_info)
{
    json_t *clean = json_object();
    copy_integer_field(clean, "screen_width", lcd_info, "screen_width");
    copy_integer_field(clean, "screen_height", lcd_info, "screen_height");
    copy_integer_field(clean, "max_image_size_bytes", lcd_info,
                       "max_image_size_bytes");
    return clean;
}

static json_t *sanitize_lcd_modes(const json_t *modes)
{
    json_t *clean_modes = json_array();
    if (!json_is_array(modes))
        return clean_modes;

    size_t index;
    json_t *mode;
    json_array_foreach(modes, index, mode)
    {
        if (!json_is_object(mode))
            continue;
        json_t *clean = json_object();
        copy_string_field(clean, "name", mode, "name");
        copy_string_field(clean, "frontend_name", mode, "frontend_name");
        copy_string_field(clean, "type", mode, "type_");
        copy_boolean_field(clean, "brightness", mode, "brightness");
        copy_boolean_field(clean, "orientation", mode, "orientation");
        copy_boolean_field(clean, "image", mode, "image");
        copy_integer_field(clean, "colors_min", mode, "colors_min");
        copy_integer_field(clean, "colors_max", mode, "colors_max");
        json_array_append_new(clean_modes, clean);
    }
    return clean_modes;
}

static json_t *sanitize_channels(const json_t *channels,
                                 ReportDevice *report_device,
                                 int *has_lcd)
{
    json_t *clean_channels = json_array();
    if (!json_is_object(channels))
        return clean_channels;

    const char *channel_name;
    json_t *channel;
    json_object_foreach((json_t *)channels, channel_name, channel)
    {
        if (!json_is_object(channel))
            continue;

        const json_t *lcd_info = json_object_get(channel, "lcd_info");
        const json_t *lcd_modes = json_object_get(channel, "lcd_modes");
        const int channel_has_lcd =
            json_is_object(lcd_info) ||
            (json_is_array(lcd_modes) && json_array_size(lcd_modes) > 0);

        json_t *clean = json_object();
        json_object_set_new(clean, "name", json_string(channel_name));
        copy_string_field(clean, "label", channel, "label");

        const json_t *speed_options = json_object_get(channel, "speed_options");
        if (json_is_object(speed_options))
        {
            json_t *speed = json_object();
            copy_boolean_field(speed, "fixed_enabled", speed_options,
                               "fixed_enabled");
            copy_integer_field(speed, "min_duty", speed_options, "min_duty");
            copy_integer_field(speed, "max_duty", speed_options, "max_duty");
            json_object_set_new(clean, "speed_options", speed);
        }

        if (json_is_object(lcd_info))
        {
            json_object_set_new(clean, "lcd_info", sanitize_lcd_info(lcd_info));
            const json_t *width = json_object_get(lcd_info, "screen_width");
            const json_t *height = json_object_get(lcd_info, "screen_height");
            if (json_is_integer(width))
                report_device->width = (int)json_integer_value(width);
            if (json_is_integer(height))
                report_device->height = (int)json_integer_value(height);
        }
        if (json_is_array(lcd_modes))
            json_object_set_new(clean, "lcd_modes",
                                sanitize_lcd_modes(lcd_modes));

        if (channel_has_lcd && !*has_lcd)
        {
            snprintf(report_device->channel, sizeof(report_device->channel),
                     "%s", channel_name);
            *has_lcd = 1;
        }
        json_array_append_new(clean_channels, clean);
    }
    return clean_channels;
}

static json_t *sanitize_status(const json_t *status_root,
                               const char *wanted_uid)
{
    const json_t *device = status_root;
    const json_t *devices = json_object_get(status_root, "devices");
    if (json_is_array(devices))
    {
        device = NULL;
        size_t index;
        json_t *candidate;
        json_array_foreach(devices, index, candidate)
        {
            const char *uid = json_string_or_null(candidate, "uid");
            if (!wanted_uid || (uid && strcmp(uid, wanted_uid) == 0))
            {
                device = candidate;
                break;
            }
        }
    }
    if (!json_is_object(device))
        return json_object();

    const json_t *history = json_object_get(device, "status_history");
    if (!json_is_array(history) || json_array_size(history) == 0)
        return json_object();

    const json_t *latest = json_array_get(history, json_array_size(history) - 1);
    json_t *clean = json_object();
    json_t *temperatures = json_array();
    json_t *channels = json_array();

    const json_t *temps = json_object_get(latest, "temps");
    if (json_is_array(temps))
    {
        size_t index;
        json_t *temperature;
        json_array_foreach(temps, index, temperature)
        {
            json_t *entry = json_object();
            copy_string_field(entry, "name", temperature, "name");
            copy_number_field(entry, "temperature", temperature, "temp");
            json_array_append_new(temperatures, entry);
        }
    }

    const json_t *raw_channels = json_object_get(latest, "channels");
    if (json_is_array(raw_channels))
    {
        size_t index;
        json_t *channel;
        json_array_foreach(raw_channels, index, channel)
        {
            json_t *entry = json_object();
            copy_string_field(entry, "name", channel, "name");
            copy_integer_field(entry, "rpm", channel, "rpm");
            copy_number_field(entry, "duty", channel, "duty");
            copy_number_field(entry, "watts", channel, "watts");
            copy_integer_field(entry, "frequency", channel, "freq");
            json_array_append_new(channels, entry);
        }
    }

    json_object_set_new(clean, "temperatures", temperatures);
    json_object_set_new(clean, "channels", channels);
    return clean;
}

static void collect_status_for_device(ReportContext *context,
                                      ReportDevice *device,
                                      json_t *clean_device)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return;
    char *escaped = curl_easy_escape(curl, device->uid, 0);
    if (!escaped)
    {
        curl_easy_cleanup(curl);
        return;
    }

    char path[512];
    int written = snprintf(path, sizeof(path), "/status/%s", escaped);
    curl_free(escaped);
    curl_easy_cleanup(curl);
    if (written < 0 || (size_t)written >= sizeof(path))
        return;

    json_t *status = get_json_endpoint(
        context, path, "Unable to collect status for one LCD device");
    if (!status)
        return;
    json_object_set_new(clean_device, "status",
                        sanitize_status(status, device->uid));
    json_decref(status);
}

static json_t *sanitize_device(ReportContext *context, const json_t *device,
                               ReportDevice *report_device, int *include)
{
    const char *uid = json_string_or_null(device, "uid");
    const char *name = json_string_or_null(device, "name");
    const char *type = json_string_or_null(device, "d_type");
    if (!type)
        type = json_string_or_null(device, "type");

    if (uid)
    {
        snprintf(report_device->uid, sizeof(report_device->uid), "%s", uid);
        add_secret(context, uid);
    }
    if (name)
        snprintf(report_device->name, sizeof(report_device->name), "%s", name);

    json_t *clean = json_object();
    json_object_set_new(clean, "id", json_string(report_device->report_id));
    if (name)
        json_object_set_new(clean, "name", json_string(name));
    if (type)
        json_object_set_new(clean, "type", json_string(type));

    const json_t *info = json_object_get(device, "info");
    int has_lcd = 0;
    if (json_is_object(info))
    {
        copy_string_field(clean, "model", info, "model");

        const json_t *driver = json_object_get(info, "driver_info");
        if (json_is_object(driver))
        {
            json_t *clean_driver = json_object();
            copy_string_field(clean_driver, "type", driver, "drv_type");
            copy_string_field(clean_driver, "name", driver, "name");
            copy_string_field(clean_driver, "version", driver, "version");
            copy_string_array_field(clean_driver, "locations", driver,
                                    "locations");
            json_object_set_new(clean, "driver", clean_driver);
        }

        const json_t *channels = json_object_get(info, "channels");
        json_object_set_new(clean, "channels",
                            sanitize_channels(channels, report_device, &has_lcd));
    }

    const json_t *liquidctl = json_object_get(device, "lc_info");
    if (json_is_object(liquidctl))
    {
        json_t *clean_liquidctl = json_object();
        copy_string_field(clean_liquidctl, "driver_type", liquidctl,
                          "driver_type");
        copy_string_field(clean_liquidctl, "firmware_version", liquidctl,
                          "firmware_version");
        copy_boolean_field(clean_liquidctl, "unknown_asetek", liquidctl,
                           "unknown_asetek");
        json_object_set_new(clean, "liquidctl", clean_liquidctl);
    }

    *include = has_lcd || (type && strcmp(type, "Liquidctl") == 0);
    return clean;
}

static json_t *collect_devices(ReportContext *context, const json_t *root)
{
    json_t *clean_devices = json_array();
    const json_t *devices = json_object_get(root, "devices");
    if (!json_is_array(devices))
    {
        add_warning(context, "CoolerControl /devices response has no device list");
        return clean_devices;
    }

    size_t index;
    size_t detected_count = 0;
    json_t *device;
    json_array_foreach(devices, index, device)
    {
        if (!json_is_object(device) ||
            context->device_count >= REPORT_MAX_DEVICES)
            continue;

        ReportDevice candidate;
        memset(&candidate, 0, sizeof(candidate));
        snprintf(candidate.report_id, sizeof(candidate.report_id), "device-%zu",
                 detected_count + 1);

        int include = 0;
        json_t *clean = sanitize_device(context, device, &candidate, &include);
        if (!include)
        {
            json_decref(clean);
            continue;
        }
        detected_count++;

        const char *requested = context->options->device_uid;
        if (requested && requested[0] != '\0' &&
            strcmp(candidate.uid, requested) != 0 &&
            strcmp(candidate.report_id, requested) != 0)
        {
            json_decref(clean);
            continue;
        }

        context->devices[context->device_count] = candidate;
        collect_status_for_device(context,
                                  &context->devices[context->device_count],
                                  clean);
        context->device_count++;
        json_array_append_new(clean_devices, clean);
    }
    if (context->options->device_uid && context->device_count == 0)
        add_warning(context, "The selected device was not detected");
    return clean_devices;
}

static void remember_usb_id(ReportContext *context, unsigned vendor,
                            unsigned product)
{
    if (!context || context->usb_id_count >= REPORT_MAX_USB_IDS)
        return;
    for (size_t i = 0; i < context->usb_id_count; i++)
    {
        if (context->usb_ids[i].vendor_id == vendor &&
            context->usb_ids[i].product_id == product)
            return;
    }
    context->usb_ids[context->usb_id_count].vendor_id = vendor;
    context->usb_ids[context->usb_id_count].product_id = product;
    context->usb_id_count++;
}

static json_t *collect_liquidctl(ReportContext *context)
{
    json_t *result = json_object();

    ReportBuffer version;
    int status = 0;
    if (buffer_init(&version, 256))
    {
        char *const argv[] = {"liquidctl", "--version", NULL};
        if (run_command(argv, &version, &status) && status == 0)
        {
            trim_line(version.data);
            char *clean = redact_text(context, version.data);
            if (clean)
            {
                json_object_set_new(result, "version", json_string(clean));
                free(clean);
            }
        }
        else
        {
            add_warning(context, "liquidctl command is unavailable");
        }
        buffer_free(&version);
    }

    ReportBuffer listing;
    if (!buffer_init(&listing, 4096))
        return result;

    char *const argv[] = {"liquidctl", "list", "--json", NULL};
    if (!run_command(argv, &listing, &status) || status != 0)
    {
        add_warning(context, "Unable to collect liquidctl list --json");
        buffer_free(&listing);
        return result;
    }

    json_error_t error;
    json_t *raw = json_loadb(listing.data, listing.size, 0, &error);
    buffer_free(&listing);
    if (!json_is_array(raw))
    {
        if (raw)
            json_decref(raw);
        add_warning(context, "liquidctl list returned an unsupported JSON shape");
        return result;
    }

    json_t *devices = json_array();
    size_t index;
    json_t *device;
    json_array_foreach(raw, index, device)
    {
        if (!json_is_object(device))
            continue;

        const char *serial = json_string_or_null(device, "serial_number");
        if (serial)
            add_secret(context, serial);

        json_t *clean = json_object();
        copy_string_field(clean, "description", device, "description");
        copy_string_field(clean, "bus", device, "bus");
        copy_string_field(clean, "driver", device, "driver");
        copy_integer_field(clean, "vendor_id", device, "vendor_id");
        copy_integer_field(clean, "product_id", device, "product_id");
        copy_integer_field(clean, "release_number", device, "release_number");

        const json_t *vendor = json_object_get(device, "vendor_id");
        const json_t *product = json_object_get(device, "product_id");
        if (json_is_integer(vendor) && json_is_integer(product))
        {
            remember_usb_id(context,
                            (unsigned)json_integer_value(vendor),
                            (unsigned)json_integer_value(product));
        }
        json_array_append_new(devices, clean);
    }
    json_object_set_new(result, "devices", devices);
    json_decref(raw);
    return result;
}

static int liquidctl_usb_id_matches(const ReportContext *context,
                                    unsigned vendor, unsigned product)
{
    for (size_t i = 0; i < context->usb_id_count; i++)
    {
        if (context->usb_ids[i].vendor_id == vendor &&
            context->usb_ids[i].product_id == product)
            return 1;
    }
    return 0;
}

static int read_hex_file(const char *path, unsigned *value)
{
    char buffer[32];
    if (!read_small_file(path, buffer, sizeof(buffer)))
        return 0;
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(buffer, &end, 16);
    if (errno != 0 || end == buffer || *end != '\0' || parsed > UINT32_MAX)
        return 0;
    *value = (unsigned)parsed;
    return 1;
}

static const char *usb_sysfs_root(void)
{
#ifdef HARDWARE_REPORT_TESTING
    const char *override = getenv("COOLERDASH_REPORT_USB_SYSFS");
    if (override && override[0] == '/')
        return override;
#endif
    return "/sys/bus/usb/devices";
}

static void copy_sysfs_attribute(json_t *destination,
                                 const char *destination_key,
                                 const char *directory,
                                 const char *filename)
{
    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/%s", directory, filename);
    if (written < 0 || (size_t)written >= sizeof(path))
        return;

    char value[512];
    if (read_small_file(path, value, sizeof(value)))
        json_object_set_new(destination, destination_key, json_string(value));
}

static void copy_sysfs_driver(json_t *destination, const char *directory)
{
    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/driver", directory);
    if (written < 0 || (size_t)written >= sizeof(path))
        return;

    char target[1024];
    ssize_t length = readlink(path, target, sizeof(target) - 1);
    if (length <= 0)
        return;
    target[length] = '\0';
    const char *name = strrchr(target, '/');
    json_object_set_new(destination, "driver",
                        json_string(name ? name + 1 : target));
}

static json_t *collect_usb_endpoints(const char *interface_path)
{
    json_t *endpoints = json_array();
    DIR *directory = opendir(interface_path);
    if (!directory)
        return endpoints;

    const struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        if (strncmp(entry->d_name, "ep_", 3) != 0)
            continue;

        char endpoint_path[1024];
        int written = snprintf(endpoint_path, sizeof(endpoint_path), "%s/%s",
                               interface_path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(endpoint_path))
            continue;

        json_t *endpoint = json_object();
        copy_sysfs_attribute(endpoint, "address", endpoint_path,
                             "bEndpointAddress");
        copy_sysfs_attribute(endpoint, "attributes", endpoint_path,
                             "bmAttributes");
        copy_sysfs_attribute(endpoint, "max_packet_size", endpoint_path,
                             "wMaxPacketSize");
        copy_sysfs_attribute(endpoint, "interval", endpoint_path, "interval");
        copy_sysfs_attribute(endpoint, "type", endpoint_path, "type");
        copy_sysfs_attribute(endpoint, "direction", endpoint_path,
                             "direction");
        json_array_append_new(endpoints, endpoint);
    }
    closedir(directory);
    return endpoints;
}

static json_t *collect_usb_interfaces(const char *root,
                                      const char *device_name)
{
    json_t *interfaces = json_array();
    DIR *directory = opendir(root);
    if (!directory)
        return interfaces;

    const size_t device_name_length = strlen(device_name);
    const struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        if (strncmp(entry->d_name, device_name, device_name_length) != 0 ||
            entry->d_name[device_name_length] != ':')
            continue;

        char interface_path[1024];
        int written = snprintf(interface_path, sizeof(interface_path), "%s/%s",
                               root, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(interface_path))
            continue;

        json_t *interface = json_object();
        copy_sysfs_attribute(interface, "number", interface_path,
                             "bInterfaceNumber");
        copy_sysfs_attribute(interface, "alternate_setting", interface_path,
                             "bAlternateSetting");
        copy_sysfs_attribute(interface, "endpoint_count", interface_path,
                             "bNumEndpoints");
        copy_sysfs_attribute(interface, "class", interface_path,
                             "bInterfaceClass");
        copy_sysfs_attribute(interface, "subclass", interface_path,
                             "bInterfaceSubClass");
        copy_sysfs_attribute(interface, "protocol", interface_path,
                             "bInterfaceProtocol");
        copy_sysfs_attribute(interface, "description", interface_path,
                             "interface");
        copy_sysfs_driver(interface, interface_path);
        json_object_set_new(interface, "endpoints",
                            collect_usb_endpoints(interface_path));
        json_array_append_new(interfaces, interface);
    }
    closedir(directory);
    return interfaces;
}

static json_t *collect_usb_devices(ReportContext *context)
{
    json_t *devices = json_array();

    const char *root = usb_sysfs_root();
    DIR *directory = opendir(root);
    if (!directory)
    {
        add_warning(context, "Unable to inspect USB devices through sysfs");
        return devices;
    }

    const struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        char path[1024];
        char device_path[1024];
        unsigned vendor = 0;
        unsigned product = 0;
        int written = snprintf(device_path, sizeof(device_path), "%s/%s",
                               root, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(device_path))
            continue;
        written = snprintf(path, sizeof(path), "%s/idVendor", device_path);
        if (written < 0 || (size_t)written >= sizeof(path) ||
            !read_hex_file(path, &vendor))
            continue;
        written = snprintf(path, sizeof(path), "%s/idProduct", device_path);
        if (written < 0 || (size_t)written >= sizeof(path) ||
            !read_hex_file(path, &product) ||
            (vendor != REPORT_NZXT_VENDOR_ID &&
             !liquidctl_usb_id_matches(context, vendor, product)))
            continue;

        json_t *clean = json_object();
        char id[16];
        snprintf(id, sizeof(id), "%04x:%04x", vendor, product);
        json_object_set_new(clean, "vid_pid", json_string(id));
        json_object_set_new(
            clean, "matched_by",
            json_string(liquidctl_usb_id_matches(context, vendor, product)
                            ? "liquidctl"
                            : "nzxt_vendor"));

        copy_sysfs_attribute(clean, "manufacturer", device_path,
                             "manufacturer");
        copy_sysfs_attribute(clean, "product", device_path, "product");
        copy_sysfs_attribute(clean, "release", device_path, "bcdDevice");
        copy_sysfs_attribute(clean, "usb_version", device_path, "version");
        copy_sysfs_attribute(clean, "speed_mbps", device_path, "speed");
        copy_sysfs_attribute(clean, "device_class", device_path,
                             "bDeviceClass");
        copy_sysfs_attribute(clean, "device_subclass", device_path,
                             "bDeviceSubClass");
        copy_sysfs_attribute(clean, "device_protocol", device_path,
                             "bDeviceProtocol");
        copy_sysfs_attribute(clean, "max_packet_size_0", device_path,
                             "bMaxPacketSize0");
        copy_sysfs_attribute(clean, "configuration_count", device_path,
                             "bNumConfigurations");
        copy_sysfs_attribute(clean, "configuration_value", device_path,
                             "bConfigurationValue");
        copy_sysfs_attribute(clean, "interface_count", device_path,
                             "bNumInterfaces");
        copy_sysfs_attribute(clean, "configuration", device_path,
                             "configuration");
        copy_sysfs_attribute(clean, "attributes", device_path,
                             "bmAttributes");
        copy_sysfs_attribute(clean, "max_power", device_path, "bMaxPower");
        json_object_set_new(clean, "interfaces",
                            collect_usb_interfaces(root, entry->d_name));

        json_array_append_new(devices, clean);
    }
    closedir(directory);
    if (json_array_size(devices) == 0)
        add_warning(context,
                    "No matching liquidctl or NZXT USB device was found");
    return devices;
}

static int log_line_is_sensitive(const char *line)
{
    const char *sensitive[] = {
        "authorization", "bearer", "password", "cookie", "access_token",
        "serial_number", "serial number", NULL};
    for (size_t i = 0; sensitive[i]; i++)
    {
        if (contains_ci(line, sensitive[i]))
            return 1;
    }
    return 0;
}

static int log_line_is_relevant(const ReportContext *context,
                                const char *line)
{
    const char *terms[] = {
        "coolerdash", "liquidctl", "lcd", "usb", "warning", "error",
        "failed", "failure", NULL};
    for (size_t i = 0; terms[i]; i++)
    {
        if (contains_ci(line, terms[i]))
            return 1;
    }
    for (size_t i = 0; i < context->device_count; i++)
    {
        if (context->devices[i].name[0] &&
            contains_ci(line, context->devices[i].name))
            return 1;
    }
    return 0;
}

static json_t *collect_filtered_logs(ReportContext *context)
{
    json_t *logs = json_array();
    ReportBuffer response;
    if (!buffer_init(&response, 8192))
        return logs;

    long code = 0;
    if (!http_request(context->config, "GET", "/logs", NULL, NULL,
                      &response, &code) ||
        code != 200)
    {
        add_warning(context, "Unable to collect CoolerControl logs");
        buffer_free(&response);
        return logs;
    }

    size_t kept_bytes = 0;
    size_t kept_lines = 0;
    char *save = NULL;
    const char *line = strtok_r(response.data, "\n", &save);
    while (line && kept_lines < REPORT_LOG_LINES &&
           kept_bytes < REPORT_LOG_LIMIT)
    {
        if (!log_line_is_sensitive(line) &&
            log_line_is_relevant(context, line))
        {
            char *clean = redact_text(context, line);
            if (clean)
            {
                size_t length = strlen(clean);
                if (kept_bytes + length <= REPORT_LOG_LIMIT)
                {
                    json_array_append_new(logs, json_string(clean));
                    kept_bytes += length;
                    kept_lines++;
                }
                free(clean);
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }
    buffer_free(&response);
    return logs;
}

static json_t *collect_display_config(const Config *config)
{
    json_t *display = json_object();
    json_object_set_new(display, "mode", json_string(config->display_mode));
    json_object_set_new(display, "width",
                        json_integer(config->display_width));
    json_object_set_new(display, "height",
                        json_integer(config->display_height));
    json_object_set_new(display, "brightness",
                        json_integer(config->lcd_brightness));
    json_object_set_new(display, "orientation",
                        json_integer(config->lcd_orientation));
    json_object_set_new(display, "refresh_interval",
                        json_real(config->display_refresh_interval));
    json_object_set_new(display, "content_scale_factor",
                        json_real(config->display_content_scale_factor));
    json_object_set_new(display, "background_image_scale_factor",
                        json_real(config->background_image_scale_factor));
    return display;
}

static ReportDevice *select_test_device(ReportContext *context,
                                        const HardwareReportOptions *options)
{
    if (context->device_count == 0)
        return NULL;

    if (options->device_uid && options->device_uid[0] != '\0')
    {
        for (size_t i = 0; i < context->device_count; i++)
        {
            if (strcmp(context->devices[i].uid, options->device_uid) == 0 ||
                strcmp(context->devices[i].report_id,
                       options->device_uid) == 0)
                return &context->devices[i];
        }
        return NULL;
    }

    if (context->device_count == 1)
        return &context->devices[0];

    if (!isatty(STDIN_FILENO))
        return NULL;

    printf("\nSelect the LCD device to test:\n");
    for (size_t i = 0; i < context->device_count; i++)
        printf("  %zu) %s\n", i + 1,
               context->devices[i].name[0] ? context->devices[i].name
                                           : context->devices[i].report_id);
    printf("Selection [1-%zu]: ", context->device_count);
    fflush(stdout);

    char input[32];
    if (!fgets(input, sizeof(input), stdin))
        return NULL;
    char *end = NULL;
    unsigned long selected = strtoul(input, &end, 10);
    if (end == input || selected == 0 || selected > context->device_count)
        return NULL;
    return &context->devices[selected - 1];
}

static int another_coolerdash_process_exists(void)
{
#ifdef HARDWARE_REPORT_TESTING
    /* Integration tests use an isolated mock API while the developer's real
     * CoolerDash service may be running in the host PID namespace. */
    const char *test_mode = getenv("COOLERDASH_REPORT_TESTING");
    if (test_mode && strcmp(test_mode, "1") == 0)
        return 0;
#endif

    DIR *proc = opendir("/proc");
    if (!proc)
        return 0;

    const pid_t self = getpid();
    const struct dirent *entry;
    while ((entry = readdir(proc)) != NULL)
    {
        if (!string_is_uint(entry->d_name))
            continue;
        pid_t pid = (pid_t)strtol(entry->d_name, NULL, 10);
        if (pid == self)
            continue;

        char path[128];
        int written = snprintf(path, sizeof(path), "/proc/%s/comm",
                               entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path))
            continue;

        char comm[128];
        if (read_small_file(path, comm, sizeof(comm)) &&
            strcmp(comm, "coolerdash") == 0)
        {
            closedir(proc);
            return 1;
        }
    }
    closedir(proc);
    return 0;
}

static int build_device_path(const char *uid, const char *suffix,
                             char *path, size_t path_size)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;
    char *escaped = curl_easy_escape(curl, uid, 0);
    if (!escaped)
    {
        curl_easy_cleanup(curl);
        return 0;
    }
    int written = snprintf(path, path_size, "/devices/%s%s", escaped, suffix);
    curl_free(escaped);
    curl_easy_cleanup(curl);
    return written > 0 && (size_t)written < path_size;
}

static json_t *get_current_lcd_settings(ReportContext *context,
                                        const ReportDevice *device)
{
    char path[512];
    if (!build_device_path(device->uid, "/settings", path, sizeof(path)))
        return NULL;
    json_t *root = get_json_endpoint(
        context, path, "Unable to read current LCD settings for the test");
    if (!root)
        return NULL;

    const json_t *settings = json_object_get(root, "settings");
    json_t *snapshot = NULL;
    if (json_is_array(settings))
    {
        size_t index;
        json_t *setting;
        json_array_foreach(settings, index, setting)
        {
            const char *channel = json_string_or_null(setting, "channel_name");
            const json_t *lcd = json_object_get(setting, "lcd");
            if (channel && strcmp(channel, device->channel) == 0 &&
                json_is_object(lcd))
            {
                snapshot = json_deep_copy(lcd);
                break;
            }
        }
    }
    json_decref(root);
    return snapshot;
}

static int put_lcd_settings(const ReportContext *context,
                            const ReportDevice *device,
                            const json_t *settings)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;
    char *escaped_uid = curl_easy_escape(curl, device->uid, 0);
    char *escaped_channel = curl_easy_escape(curl, device->channel, 0);
    if (!escaped_uid || !escaped_channel)
    {
        if (escaped_uid)
            curl_free(escaped_uid);
        if (escaped_channel)
            curl_free(escaped_channel);
        curl_easy_cleanup(curl);
        return 0;
    }

    char path[768];
    int written = snprintf(path, sizeof(path),
                           "/devices/%s/settings/%s/lcd",
                           escaped_uid, escaped_channel);
    curl_free(escaped_uid);
    curl_free(escaped_channel);
    curl_easy_cleanup(curl);
    if (written < 0 || (size_t)written >= sizeof(path))
        return 0;

    char *body = json_dumps(settings, JSON_COMPACT);
    if (!body)
        return 0;

    ReportBuffer response;
    if (!buffer_init(&response, 1024))
    {
        free(body);
        return 0;
    }
    long code = 0;
    int ok = http_request(context->config, "PUT", path, body,
                          "application/json", &response, &code);
    buffer_free(&response);
    free(body);
    return ok && (code == 200 || code == 204);
}

static int create_lcd_test_image(const char *path, int width, int height)
{
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(surface);
        return 0;
    }
    cairo_t *cr = cairo_create(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    const double half_width = width / 2.0;
    const double half_height = height / 2.0;
    const double colors[4][3] = {
        {0.85, 0.10, 0.10}, {0.10, 0.65, 0.15},
        {0.10, 0.25, 0.85}, {0.95, 0.75, 0.05}};
    for (int row = 0; row < 2; row++)
    {
        for (int column = 0; column < 2; column++)
        {
            int color = row * 2 + column;
            cairo_set_source_rgb(cr, colors[color][0],
                                 colors[color][1], colors[color][2]);
            cairo_rectangle(cr, column * half_width, row * half_height,
                            half_width, half_height);
            cairo_fill(cr);
        }
    }

    double border = (width < height ? width : height) * 0.025;
    if (border < 3.0)
        border = 3.0;
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, border);
    cairo_rectangle(cr, border / 2.0, border / 2.0,
                    width - border, height - border);
    cairo_stroke(cr);

    const double center_x = width / 2.0;
    const double center_y = height / 2.0;
    const double base_radius = (width < height ? width : height) / 2.0;
    const double guide_ratios[] = {0.98, 0.94, 0.90};
    const double guide_colors[][3] = {
        {0.0, 1.0, 1.0}, {1.0, 1.0, 0.0}, {1.0, 0.0, 1.0}};
    cairo_set_line_width(cr, border * 0.35);
    for (size_t i = 0; i < 3; i++)
    {
        cairo_set_source_rgb(cr, guide_colors[i][0], guide_colors[i][1],
                             guide_colors[i][2]);
        cairo_arc(cr, center_x, center_y, base_radius * guide_ratios[i],
                  0.0, 2.0 * 3.14159265358979323846);
        cairo_stroke(cr);
    }

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, border * 0.25);
    cairo_move_to(cr, center_x - base_radius * 0.12, center_y);
    cairo_line_to(cr, center_x + base_radius * 0.12, center_y);
    cairo_move_to(cr, center_x, center_y - base_radius * 0.12);
    cairo_line_to(cr, center_x, center_y + base_radius * 0.12);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, (width < height ? width : height) * 0.09);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    char label[128];
    snprintf(label, sizeof(label), "CoolerDash %dx%d", width, height);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label, &extents);
    cairo_move_to(cr, (width - extents.width) / 2.0 - extents.x_bearing,
                  height * 0.68 - extents.y_bearing);
    cairo_show_text(cr, label);

    cairo_surface_flush(surface);
    cairo_status_t status = cairo_surface_write_to_png(surface, path);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    if (status != CAIRO_STATUS_SUCCESS)
        return 0;
    return chmod(path, 0644) == 0;
}

static void set_test_status(json_t *test, const char *status,
                            const char *reason)
{
    json_object_set_new(test, "status", json_string(status));
    if (reason)
        json_object_set_new(test, "reason", json_string(reason));
}

static void record_lcd_geometry_observations(json_t *test)
{
    char input[32];

    printf("\nWhat is the physically visible LCD shape?\n");
    printf("  1) Circular\n");
    printf("  2) Square or rectangular\n");
    printf("  3) Rounded square\n");
    printf("  4) Unsure\n");
    printf("Selection [1-4]: ");
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin) || lcd_test_interrupted)
        return;
    const char *shape = "unknown";
    if (input[0] == '1')
        shape = "circular";
    else if (input[0] == '2')
        shape = "rectangular";
    else if (input[0] == '3')
        shape = "rounded_square";
    json_object_set_new(test, "observed_shape", json_string(shape));

    printf("\nWhich colored circle is the largest one fully visible?\n");
    printf("  1) Cyan (98%%)\n");
    printf("  2) Yellow (94%%)\n");
    printf("  3) Magenta (90%%)\n");
    printf("  4) None or unsure\n");
    printf("Selection [1-4]: ");
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin) || lcd_test_interrupted)
        return;
    double ratio = 0.0;
    if (input[0] == '1')
        ratio = 0.98;
    else if (input[0] == '2')
        ratio = 0.94;
    else if (input[0] == '3')
        ratio = 0.90;
    if (ratio > 0.0)
        json_object_set_new(test, "largest_visible_circle_ratio",
                            json_real(ratio));

    printf("\nIs the white center cross centered in the visible panel?\n");
    printf("  1) Yes\n");
    printf("  2) No\n");
    printf("  3) Unsure\n");
    printf("Selection [1-3]: ");
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin) || lcd_test_interrupted)
        return;
    const char *centering = "unknown";
    if (input[0] == '1')
        centering = "centered";
    else if (input[0] == '2')
        centering = "off_center";
    json_object_set_new(test, "observed_centering", json_string(centering));
}

static json_t *run_optional_lcd_test(
    ReportContext *context, const HardwareReportOptions *options)
{
    json_t *test = json_object();
    json_object_set_new(test, "requested", json_true());

    if (!isatty(STDIN_FILENO))
    {
        set_test_status(test, "skipped", "interactive terminal required");
        return test;
    }
    if (another_coolerdash_process_exists())
    {
        set_test_status(test, "skipped",
                        "another CoolerDash process is running");
        return test;
    }

    ReportDevice *device = select_test_device(context, options);
    if (!device)
    {
        set_test_status(test, "skipped",
                        "no unique LCD device was selected");
        return test;
    }
    json_object_set_new(test, "device", json_string(device->report_id));
    if (device->width <= 0 || device->height <= 0 ||
        device->channel[0] == '\0')
    {
        set_test_status(test, "skipped",
                        "device has no usable LCD metadata");
        return test;
    }
    json_object_set_new(test, "image_width", json_integer(device->width));
    json_object_set_new(test, "image_height", json_integer(device->height));
    if (access(context->config->paths_images, W_OK) != 0)
    {
        set_test_status(test, "skipped",
                        "CoolerDash image directory is not writable");
        return test;
    }

    json_t *snapshot = get_current_lcd_settings(context, device);
    if (!snapshot)
    {
        set_test_status(test, "skipped",
                        "current LCD settings could not be saved");
        return test;
    }

    printf("\nThe optional test changes only the selected LCD image.\n");
    printf("Current LCD settings will be restored afterwards.\n");
    printf("Type TEST to continue: ");
    fflush(stdout);
    char input[32];
    if (!fgets(input, sizeof(input), stdin))
    {
        json_decref(snapshot);
        set_test_status(test, "cancelled", "confirmation was not provided");
        return test;
    }
    trim_line(input);
    if (strcmp(input, "TEST") != 0)
    {
        json_decref(snapshot);
        set_test_status(test, "cancelled", "confirmation was not provided");
        return test;
    }

    char image_path[HARDWARE_REPORT_PATH_SIZE];
    int written = snprintf(image_path, sizeof(image_path),
                           "%s/hardware-report-test-%ld.png",
                           context->config->paths_images, (long)getpid());
    if (written < 0 || (size_t)written >= sizeof(image_path) ||
        !create_lcd_test_image(image_path, device->width, device->height))
    {
        json_decref(snapshot);
        set_test_status(test, "failed", "test image could not be created");
        return test;
    }

    json_t *test_settings = json_object();
    json_object_set_new(test_settings, "mode", json_string("image"));
    json_object_set_new(test_settings, "image_file_processed",
                        json_string(image_path));
    json_object_set_new(test_settings, "brightness",
                        json_integer(context->config->lcd_brightness));
    json_object_set_new(test_settings, "orientation",
                        json_integer(context->config->lcd_orientation));
    json_object_set_new(test_settings, "colors", json_array());

    struct sigaction action;
    struct sigaction old_int;
    struct sigaction old_term;
    memset(&action, 0, sizeof(action));
    action.sa_handler = lcd_test_signal_handler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, &old_int);
    sigaction(SIGTERM, &action, &old_term);
    lcd_test_interrupted = 0;

    int uploaded = put_lcd_settings(context, device, test_settings);
    json_decref(test_settings);
    if (uploaded && !lcd_test_interrupted)
    {
        printf("\nHow does the test image look?\n");
        printf("  1) Correct\n");
        printf("  2) Rotated\n");
        printf("  3) Distorted or incorrectly scaled\n");
        printf("  4) Not visible\n");
        printf("Selection [1-4]: ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin))
        {
            switch (input[0])
            {
            case '1':
                json_object_set_new(test, "result", json_string("correct"));
                break;
            case '2':
                json_object_set_new(test, "result", json_string("rotated"));
                break;
            case '3':
                json_object_set_new(test, "result",
                                    json_string("distorted_or_scaled"));
                break;
            case '4':
                json_object_set_new(test, "result", json_string("not_visible"));
                break;
            default:
                json_object_set_new(test, "result", json_string("unknown"));
                break;
            }
            if (!lcd_test_interrupted && input[0] >= '1' && input[0] <= '3')
                record_lcd_geometry_observations(test);
        }
    }

    int restored = put_lcd_settings(context, device, snapshot);
    json_decref(snapshot);
    unlink(image_path);
    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGTERM, &old_term, NULL);

    json_object_set_new(test, "restored", json_boolean(restored));
    if (!restored)
    {
        fprintf(stderr,
                "Warning: LCD settings could not be restored. Restart "
                "CoolerControl/CoolerDash.\n");
        set_test_status(test, "failed", "LCD settings restore failed");
    }
    else if (lcd_test_interrupted)
    {
        set_test_status(test, "cancelled", "test interrupted");
    }
    else if (!uploaded)
    {
        set_test_status(test, "failed", "test image upload failed");
    }
    else
    {
        set_test_status(test, "completed", NULL);
    }
    return test;
}

static void redact_json_strings(const ReportContext *context, json_t *value)
{
    if (json_is_object(value))
    {
        const char *key;
        json_t *child;
        json_object_foreach(value, key, child)
        {
            (void)key;
            if (json_is_string(child))
            {
                char *clean = redact_text(context, json_string_value(child));
                if (clean)
                {
                    json_string_set(child, clean);
                    free(clean);
                }
            }
            else
            {
                redact_json_strings(context, child);
            }
        }
    }
    else if (json_is_array(value))
    {
        size_t index;
        json_t *child;
        json_array_foreach(value, index, child)
        {
            if (json_is_string(child))
            {
                char *clean = redact_text(context, json_string_value(child));
                if (clean)
                {
                    json_string_set(child, clean);
                    free(clean);
                }
            }
            else
            {
                redact_json_strings(context, child);
            }
        }
    }
}

static int append_format(ReportBuffer *buffer, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    int required = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (required < 0)
    {
        va_end(arguments);
        return 0;
    }

    char *formatted = malloc((size_t)required + 1);
    if (!formatted)
    {
        va_end(arguments);
        return 0;
    }
    vsnprintf(formatted, (size_t)required + 1, format, arguments);
    va_end(arguments);
    int result = buffer_append(buffer, formatted, (size_t)required);
    free(formatted);
    return result;
}

static const char *json_object_string(const json_t *object, const char *key,
                                      const char *fallback)
{
    const char *value = json_string_or_null(object, key);
    return value ? value : fallback;
}

static long long json_object_integer(const json_t *object, const char *key)
{
    const json_t *value = json_object_get(object, key);
    return json_is_integer(value) ? json_integer_value(value) : 0;
}

static char *render_markdown(const json_t *root)
{
    ReportBuffer markdown;
    if (!buffer_init(&markdown, 8192))
        return NULL;

    const json_t *system = json_object_get(root, "system");
    const json_t *coolercontrol = json_object_get(root, "coolercontrol");
    const json_t *health = json_object_get(coolercontrol, "health");
    const json_t *details = json_object_get(health, "details");
    const json_t *devices = json_object_get(coolercontrol, "devices");
    const json_t *liquidctl = json_object_get(root, "liquidctl");
    const json_t *usb = json_object_get(root, "usb_devices");
    const json_t *test = json_object_get(root, "lcd_test");
    const json_t *collection = json_object_get(root, "collection");
    const json_t *warnings = json_object_get(collection, "warnings");

    append_format(&markdown,
                  "# CoolerDash Hardware Report\n\n"
                  "- Generated: %s\n"
                  "- CoolerDash: %s\n"
                  "- Distribution: %s\n"
                  "- Kernel: %s (%s)\n"
                  "- CoolerControl: %s\n"
                  "- liquidctl: %s\n\n",
                  json_object_string(root, "generated_at", "unknown"),
                  json_object_string(root, "coolerdash_version", "unknown"),
                  json_object_string(system, "distribution", "unknown"),
                  json_object_string(system, "kernel", "unknown"),
                  json_object_string(system, "architecture", "unknown"),
                  json_object_string(details, "version", "unavailable"),
                  json_object_string(liquidctl, "version", "unavailable"));

    append_format(&markdown, "## Detected LCD devices\n\n");
    if (!json_is_array(devices) || json_array_size(devices) == 0)
    {
        append_format(&markdown, "No Liquidctl/LCD device was detected.\n\n");
    }
    else
    {
        append_format(&markdown,
                      "| ID | Device | Type | Driver | LCD |\n"
                      "|---|---|---|---|---|\n");
        size_t index;
        json_t *device;
        json_array_foreach(devices, index, device)
        {
            const json_t *driver = json_object_get(device, "driver");
            const json_t *channels = json_object_get(device, "channels");
            int width = 0;
            int height = 0;
            if (json_is_array(channels))
            {
                size_t channel_index;
                json_t *channel;
                json_array_foreach(channels, channel_index, channel)
                {
                    const json_t *lcd_info =
                        json_object_get(channel, "lcd_info");
                    if (json_is_object(lcd_info))
                    {
                        width = (int)json_object_integer(lcd_info,
                                                         "screen_width");
                        height = (int)json_object_integer(lcd_info,
                                                          "screen_height");
                        break;
                    }
                }
            }
            char dimensions[64] = "not reported";
            if (width > 0 && height > 0)
                snprintf(dimensions, sizeof(dimensions), "%dx%d", width, height);
            append_format(
                &markdown, "| %s | %s | %s | %s %s | %s |\n",
                json_object_string(device, "id", "unknown"),
                json_object_string(device, "name", "unknown"),
                json_object_string(device, "type", "unknown"),
                json_object_string(driver, "name", "unknown"),
                json_object_string(driver, "version", ""),
                dimensions);
        }
        append_format(&markdown, "\n");
    }

    append_format(&markdown, "## USB matches\n\n");
    if (!json_is_array(usb) || json_array_size(usb) == 0)
    {
        append_format(&markdown, "No matching USB descriptor was collected.\n\n");
    }
    else
    {
        append_format(&markdown,
                      "| VID:PID | Match | Manufacturer | Product | Release | USB | Interfaces |\n"
                      "|---|---|---|---|---|---|---|\n");
        size_t index;
        json_t *device;
        json_array_foreach(usb, index, device)
        {
            const json_t *interfaces = json_object_get(device, "interfaces");
            size_t interface_count = json_is_array(interfaces)
                                         ? json_array_size(interfaces)
                                         : 0;
            append_format(
                &markdown, "| %s | %s | %s | %s | %s | %s Mbps | %zu |\n",
                json_object_string(device, "vid_pid", "unknown"),
                json_object_string(device, "matched_by", "unknown"),
                json_object_string(device, "manufacturer", "unknown"),
                json_object_string(device, "product", "unknown"),
                json_object_string(device, "release", "unknown"),
                json_object_string(device, "speed_mbps", "unknown"),
                interface_count);
        }
        append_format(&markdown, "\n");
    }

    append_format(&markdown, "## LCD test\n\n");
    append_format(&markdown, "- Status: %s\n",
                  json_object_string(test, "status", "not requested"));
    const char *result = json_string_or_null(test, "result");
    const char *reason = json_string_or_null(test, "reason");
    if (result)
        append_format(&markdown, "- Result: %s\n", result);
    if (reason)
        append_format(&markdown, "- Reason: %s\n", reason);
    const char *observed_shape = json_string_or_null(test, "observed_shape");
    const char *observed_centering =
        json_string_or_null(test, "observed_centering");
    const json_t *visible_circle =
        json_object_get(test, "largest_visible_circle_ratio");
    if (observed_shape)
        append_format(&markdown, "- Observed shape: %s\n", observed_shape);
    if (json_is_number(visible_circle))
        append_format(&markdown, "- Largest fully visible guide circle: %.0f%%\n",
                      json_number_value(visible_circle) * 100.0);
    if (observed_centering)
        append_format(&markdown, "- Observed centering: %s\n",
                      observed_centering);
    append_format(&markdown, "\n");

    append_format(&markdown, "## Collection warnings\n\n");
    if (!json_is_array(warnings) || json_array_size(warnings) == 0)
    {
        append_format(&markdown, "None.\n\n");
    }
    else
    {
        size_t index;
        json_t *warning;
        json_array_foreach(warnings, index, warning)
        {
            if (json_is_string(warning))
                append_format(&markdown, "- %s\n",
                              json_string_value(warning));
        }
        append_format(&markdown, "\n");
    }

    append_format(
        &markdown,
        "## Submission\n\n"
        "Review both report files, attach the JSON file to the CoolerDash "
        "device issue, and paste this Markdown summary into the issue.\n");
    return markdown.data;
}

static int report_owner(uid_t *uid, gid_t *gid)
{
    *uid = getuid();
    *gid = getgid();
    if (geteuid() != 0)
        return 1;

    const char *sudo_uid = getenv("SUDO_UID");
    const char *sudo_gid = getenv("SUDO_GID");
    if (!string_is_uint(sudo_uid) || !string_is_uint(sudo_gid))
        return 1;

    unsigned long parsed_uid = strtoul(sudo_uid, NULL, 10);
    unsigned long parsed_gid = strtoul(sudo_gid, NULL, 10);
    if (parsed_uid > (unsigned long)(uid_t)-1 ||
        parsed_gid > (unsigned long)(gid_t)-1)
        return 0;
    *uid = (uid_t)parsed_uid;
    *gid = (gid_t)parsed_gid;
    return 1;
}

static int write_atomic_file(const char *path, const char *contents,
                             size_t length)
{
    char temporary[HARDWARE_REPORT_PATH_SIZE + 64];
    int written = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld",
                           path, (long)getpid());
    if (written < 0 || (size_t)written >= sizeof(temporary))
        return 0;

    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return 0;

    size_t offset = 0;
    while (offset < length)
    {
        ssize_t count = write(fd, contents + offset, length - offset);
        if (count < 0)
        {
            if (errno == EINTR)
                continue;
            close(fd);
            unlink(temporary);
            return 0;
        }
        offset += (size_t)count;
    }

    int ok = fsync(fd) == 0;
    uid_t uid;
    gid_t gid;
    if (ok && report_owner(&uid, &gid) &&
        geteuid() == 0 && (uid != 0 || gid != 0))
        ok = fchown(fd, uid, gid) == 0;
    if (close(fd) != 0)
        ok = 0;
    if (ok)
        ok = rename(temporary, path) == 0;
    if (!ok)
        unlink(temporary);
    return ok;
}

static int validate_output_directory(const char *path)
{
    struct stat metadata;
    return path && stat(path, &metadata) == 0 &&
           S_ISDIR(metadata.st_mode) && access(path, W_OK) == 0;
}

static void make_timestamp(char *iso, size_t iso_size,
                           char *filename, size_t filename_size)
{
    time_t now = time(NULL);
    struct tm utc;
    if (!gmtime_r(&now, &utc))
    {
        snprintf(iso, iso_size, "unknown");
        snprintf(filename, filename_size, "%ld", (long)now);
        return;
    }
    strftime(iso, iso_size, "%Y-%m-%dT%H:%M:%SZ", &utc);
    strftime(filename, filename_size, "%Y%m%d-%H%M%S", &utc);
}

int run_hardware_report(const Config *config,
                        const HardwareReportOptions *options,
                        const char *coolerdash_version)
{
    if (!config || !options || !coolerdash_version)
        return 0;

    ReportContext context;
    memset(&context, 0, sizeof(context));
    context.config = config;
    context.options = options;
    context.root = json_object();
    context.warnings = json_array();
    if (!context.root || !context.warnings)
    {
        if (context.root)
            json_decref(context.root);
        if (context.warnings)
            json_decref(context.warnings);
        return 0;
    }

    if (!hardware_report_default_output_dir(context.home_dir,
                                            sizeof(context.home_dir)))
        context.home_dir[0] = '\0';
    add_secret(&context, context.home_dir);
    add_secret(&context, config->access_token);
    add_secret(&context, config->daemon_address);

    uid_t invoking_uid = getuid();
    if (geteuid() == 0 && string_is_uint(getenv("SUDO_UID")))
        invoking_uid = (uid_t)strtoul(getenv("SUDO_UID"), NULL, 10);
    const struct passwd *invoking_user = getpwuid(invoking_uid);
    if (invoking_user)
        add_secret(&context, invoking_user->pw_name);

    char generated_at[64];
    char file_timestamp[64];
    make_timestamp(generated_at, sizeof(generated_at),
                   file_timestamp, sizeof(file_timestamp));
    json_object_set_new(context.root, "schema_version",
                        json_integer(REPORT_SCHEMA_VERSION));
    json_object_set_new(context.root, "generated_at",
                        json_string(generated_at));
    json_object_set_new(context.root, "coolerdash_version",
                        json_string(coolerdash_version));
    json_object_set_new(context.root, "system",
                        collect_system_info(&context));
    json_object_set_new(context.root, "display_configuration",
                        collect_display_config(config));

    json_t *coolercontrol = json_object();
    json_t *health = get_json_endpoint(
        &context, "/health", "Unable to collect CoolerControl health data");
    if (health)
    {
        json_object_set_new(coolercontrol, "health", sanitize_health(health));
        json_decref(health);
    }
    else
    {
        json_object_set_new(coolercontrol, "health", json_object());
    }

    json_t *raw_devices = get_json_endpoint(
        &context, "/devices", "Unable to collect CoolerControl device data");
    if (raw_devices)
    {
        json_object_set_new(coolercontrol, "devices",
                            collect_devices(&context, raw_devices));
        json_decref(raw_devices);
    }
    else
    {
        json_object_set_new(coolercontrol, "devices", json_array());
    }

    json_object_set_new(context.root, "liquidctl",
                        collect_liquidctl(&context));
    json_object_set_new(context.root, "usb_devices",
                        collect_usb_devices(&context));
    json_object_set_new(coolercontrol, "filtered_logs",
                        collect_filtered_logs(&context));
    json_object_set_new(context.root, "coolercontrol", coolercontrol);

    if (options->test_lcd)
        json_object_set_new(context.root, "lcd_test",
                            run_optional_lcd_test(&context, options));
    else
    {
        json_t *test = json_object();
        json_object_set_new(test, "requested", json_false());
        set_test_status(test, "not_requested", NULL);
        json_object_set_new(context.root, "lcd_test", test);
    }

    json_t *collection = json_object();
    json_object_set_new(collection, "warnings", context.warnings);
    context.warnings = NULL;
    json_object_set_new(context.root, "collection", collection);
    redact_json_strings(&context, context.root);

    const char *output_dir = options->output_dir;
    if (!output_dir || output_dir[0] == '\0')
        output_dir = context.home_dir;
    if (!validate_output_directory(output_dir))
    {
        fprintf(stderr, "Error: report output directory is not writable: %s\n",
                output_dir && output_dir[0] ? output_dir : "(unknown)");
        json_decref(context.root);
        free_secrets(&context);
        return 0;
    }

    char json_path[HARDWARE_REPORT_PATH_SIZE];
    char markdown_path[HARDWARE_REPORT_PATH_SIZE];
    int json_written = snprintf(
        json_path, sizeof(json_path), "%s/coolerdash-hardware-report-%s.json",
        output_dir, file_timestamp);
    int markdown_written = snprintf(
        markdown_path, sizeof(markdown_path),
        "%s/coolerdash-hardware-report-%s.md",
        output_dir, file_timestamp);
    if (json_written < 0 || (size_t)json_written >= sizeof(json_path) ||
        markdown_written < 0 ||
        (size_t)markdown_written >= sizeof(markdown_path))
    {
        fprintf(stderr, "Error: report output path is too long\n");
        json_decref(context.root);
        free_secrets(&context);
        return 0;
    }

    char *json_output =
        json_dumps(context.root, JSON_INDENT(2) | JSON_SORT_KEYS);
    char *markdown_output = render_markdown(context.root);
    if (!json_output || !markdown_output)
    {
        free(json_output);
        free(markdown_output);
        json_decref(context.root);
        free_secrets(&context);
        return 0;
    }

    int json_ok = write_atomic_file(json_path, json_output,
                                    strlen(json_output));
    int markdown_ok = write_atomic_file(markdown_path, markdown_output,
                                        strlen(markdown_output));
    free(json_output);
    free(markdown_output);
    json_decref(context.root);
    free_secrets(&context);

    if (!json_ok || !markdown_ok)
    {
        fprintf(stderr, "Error: could not write both report files\n");
        return 0;
    }

    printf("\nHardware report created locally:\n");
    printf("  JSON: %s\n", json_path);
    printf("  Markdown: %s\n", markdown_path);
    printf("Review both files before attaching them to a GitHub issue.\n");
    return 1;
}
