#define _POSIX_C_SOURCE 200809L
#include "shutdown.h"
#include "config.h"
#include "../mods/display.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <jansson.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IMPORT_MAX_BYTES (16U * 1024U * 1024U)
#define IMPORT_CHUNK_BYTES (256U * 1024U)
#define IMPORT_TEMP_PATH DEFAULT_COOLERDASH_PLUGIN_DIR "/.user-shutdown-image.upload"

typedef struct
{
    int fd;
    size_t expected;
    size_t received;
    char id[33];
} ImportState;

static ImportState s_import = {.fd = -1};
static char s_token[33];

static int random_hex(char output[33])
{
    unsigned char bytes[16];
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    size_t offset = 0;
    while (offset < sizeof(bytes))
    {
        ssize_t n = read(fd, bytes + offset, sizeof(bytes) - offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
        {
            close(fd);
            return 0;
        }
        offset += (size_t)n;
    }
    close(fd);
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(bytes); ++i)
    {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 15];
    }
    output[32] = '\0';
    return 1;
}

int shutdown_image_action_token_valid(const char *token)
{
    if (!token || strlen(token) != 32 || !s_token[0])
        return 0;
    unsigned int diff = 0;
    for (size_t i = 0; i < 32; ++i)
        diff |= (unsigned char)token[i] ^ (unsigned char)s_token[i];
    return diff == 0;
}

char *shutdown_image_action_token_json(void)
{
    if (!s_token[0] && !random_hex(s_token))
        return NULL;
    json_t *root = json_pack("{s:s}", "token", s_token);
    if (!root)
        return NULL;
    char *result = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    return result;
}

void shutdown_image_import_cleanup(void)
{
    if (s_import.fd >= 0)
        close(s_import.fd);
    s_import.fd = -1;
    s_import.expected = 0;
    s_import.received = 0;
    s_import.id[0] = '\0';
    (void)unlink(IMPORT_TEMP_PATH);
}

static char *response_error(int *status, int code, const char *message)
{
    *status = code;
    json_t *root = json_pack("{s:s}", "error", message);
    if (!root)
        return NULL;
    char *result = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    return result;
}

static int write_chunk(int fd, const unsigned char *data, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        ssize_t n = write(fd, data + offset, length - offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return 0;
        offset += (size_t)n;
    }
    return 1;
}

char *shutdown_image_import_json(const char *body, size_t length, int *status)
{
    *status = 422;
    if (!body || length == 0 || length > 512U * 1024U)
        return response_error(status, 413, "Invalid upload request size");
    json_error_t error;
    json_t *root = json_loadb(body, length, JSON_REJECT_DUPLICATES, &error);
    if (!root)
        return response_error(status, 422, "Invalid upload request");
    const char *op = json_string_value(json_object_get(root, "op"));
    const char *token = json_string_value(json_object_get(root, "token"));
    char *result = NULL;
    if (!shutdown_image_action_token_valid(token))
    {
        result = response_error(status, 403, "Action token is invalid");
        goto done;
    }
    if (op && strcmp(op, "begin") == 0)
    {
        json_t *size = json_object_get(root, "size");
        if (!json_is_integer(size) || json_integer_value(size) <= 0 ||
            json_integer_value(size) > IMPORT_MAX_BYTES)
        {
            result = response_error(status, 422, "Image must be at most 16 MiB");
            goto done;
        }
        shutdown_image_import_cleanup();
        if (!random_hex(s_import.id))
        {
            result = response_error(status, 500, "Cannot start image import");
            goto done;
        }
        s_import.fd = open(IMPORT_TEMP_PATH, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (s_import.fd < 0)
        {
            s_import.id[0] = '\0';
            result = response_error(status, 500, "Cannot write to the plugin directory");
            goto done;
        }
        s_import.expected = (size_t)json_integer_value(size);
        *status = 200;
        json_t *reply = json_pack("{s:s}", "id", s_import.id);
        result = reply ? json_dumps(reply, JSON_COMPACT) : NULL;
        json_decref(reply);
    }
    else
    {
        const char *id = json_string_value(json_object_get(root, "id"));
        if (!id || !s_import.id[0] || strcmp(id, s_import.id) != 0 || s_import.fd < 0)
        {
            result = response_error(status, 409, "No matching image import");
            goto done;
        }
        if (op && strcmp(op, "chunk") == 0)
        {
            const char *encoded = json_string_value(json_object_get(root, "data"));
            if (!encoded || strlen(encoded) > ((IMPORT_CHUNK_BYTES + 2) / 3) * 4 ||
                strlen(encoded) % 4 != 0)
            {
                result = response_error(status, 422, "Invalid image chunk");
                goto done;
            }
            gsize decoded_size = 0;
            guchar *decoded = g_base64_decode(encoded, &decoded_size);
            if (!decoded || decoded_size == 0 || decoded_size > IMPORT_CHUNK_BYTES ||
                decoded_size > s_import.expected - s_import.received ||
                !write_chunk(s_import.fd, decoded, decoded_size))
            {
                g_free(decoded);
                shutdown_image_import_cleanup();
                result = response_error(status, 422, "Image transfer failed");
                goto done;
            }
            g_free(decoded);
            s_import.received += decoded_size;
            *status = 200;
            result = strdup("{}\n");
        }
        else if (op && strcmp(op, "finish") == 0)
        {
            int complete = s_import.received == s_import.expected;
            if (complete && fchmod(s_import.fd, 0644) != 0)
                complete = 0;
            if (complete && fsync(s_import.fd) != 0)
                complete = 0;
            if (close(s_import.fd) != 0)
                complete = 0;
            s_import.fd = -1;
            if (!complete)
            {
                shutdown_image_import_cleanup();
                result = response_error(status, 422, "Incomplete image upload");
                goto done;
            }
            if (!image_file_is_supported(IMPORT_TEMP_PATH) ||
                rename(IMPORT_TEMP_PATH, USER_SHUTDOWN_IMAGE_PATH) != 0)
            {
                shutdown_image_import_cleanup();
                result = response_error(status, 422, "Unsupported image or cannot save it");
                goto done;
            }
            int dir_fd = open(DEFAULT_COOLERDASH_PLUGIN_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (dir_fd >= 0)
            {
                (void)fsync(dir_fd);
                close(dir_fd);
            }
            s_import.id[0] = '\0';
            *status = 200;
            json_t *reply = json_pack("{s:s}", "path", USER_SHUTDOWN_IMAGE_PATH);
            result = reply ? json_dumps(reply, JSON_COMPACT) : NULL;
            json_decref(reply);
        }
        else if (op && strcmp(op, "cancel") == 0)
        {
            shutdown_image_import_cleanup();
            *status = 200;
            result = strdup("{}\n");
        }
        else
            result = response_error(status, 422, "Unknown import action");
    }
done:
    json_decref(root);
    return result;
}
