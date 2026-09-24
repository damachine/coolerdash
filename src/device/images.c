#define _POSIX_C_SOURCE 200809L
#include "images.h"
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
#define SHUTDOWN_IMPORT_TEMP_PATH DEFAULT_COOLERDASH_PLUGIN_DIR "/.user-shutdown-image.upload"
#define BACKGROUND_IMPORT_TEMP_PATH DEFAULT_COOLERDASH_PLUGIN_DIR "/.user-background-image.upload"

static const char *import_temp_path(int background)
{
    return background ? BACKGROUND_IMPORT_TEMP_PATH : SHUTDOWN_IMPORT_TEMP_PATH;
}

static const char *import_target_path(int background)
{
    return background ? USER_BACKGROUND_IMAGE_PATH : USER_SHUTDOWN_IMAGE_PATH;
}

typedef struct
{
    int fd;
    size_t expected;
    size_t received;
    int background;
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

int image_action_token_valid(const char *token)
{
    if (!token || strlen(token) != 32 || !s_token[0])
        return 0;
    unsigned int diff = 0;
    for (size_t i = 0; i < 32; ++i)
        diff |= (unsigned char)token[i] ^ (unsigned char)s_token[i];
    return diff == 0;
}

char *image_action_token_json(void)
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

void image_import_cleanup(void)
{
    if (s_import.fd >= 0)
        close(s_import.fd);
    (void)unlink(SHUTDOWN_IMPORT_TEMP_PATH);
    (void)unlink(BACKGROUND_IMPORT_TEMP_PATH);
    s_import.fd = -1;
    s_import.expected = 0;
    s_import.received = 0;
    s_import.background = 0;
    s_import.id[0] = '\0';
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

static char *image_import_json(const char *body, size_t length, int *status,
                               int background)
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
    if (!image_action_token_valid(token))
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
        image_import_cleanup();
        s_import.background = background;
        if (!random_hex(s_import.id))
        {
            result = response_error(status, 500, "Cannot start image import");
            goto done;
        }
        s_import.fd = open(import_temp_path(background),
                           O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
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
        if (!id || !s_import.id[0] || strcmp(id, s_import.id) != 0 ||
            s_import.fd < 0 || s_import.background != background)
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
                image_import_cleanup();
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
                image_import_cleanup();
                result = response_error(status, 422, "Incomplete image upload");
                goto done;
            }
            if (!image_file_is_supported(import_temp_path(background)) ||
                rename(import_temp_path(background), import_target_path(background)) != 0)
            {
                image_import_cleanup();
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
            json_t *reply = json_pack("{s:s}", "path", import_target_path(background));
            result = reply ? json_dumps(reply, JSON_COMPACT) : NULL;
            json_decref(reply);
        }
        else if (op && strcmp(op, "cancel") == 0)
        {
            image_import_cleanup();
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

char *shutdown_image_import_json(const char *body, size_t length, int *status)
{
    return image_import_json(body, length, status, 0);
}

char *background_image_import_json(const char *body, size_t length, int *status)
{
    return image_import_json(body, length, status, 1);
}
