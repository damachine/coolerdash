/* SPDX-License-Identifier: MIT */
#include "lcd.h"

#include "config.h"
#include <glib.h>
#include <hidapi.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>

enum { NZXT_BOOTLOADER_PID = 0x3011, LCD_INTERFACE = 0, LCD_ENDPOINT = 0x02,
       LCD_TIMEOUT_MS = 500, LCD_MAX_PAYLOAD = 1200000 };

struct StreamLcd
{
    const DisplayProfile *profile;
    libusb_context *usb;
    libusb_device_handle *bulk;
    hid_device *hid;
    uint8_t *payload;
    size_t capacity;
    int claimed;
    int first_frame_sent;
};

void lcd_stream_close(StreamLcd *lcd)
{
    if (!lcd)
        return;
    if (lcd->hid)
        hid_close(lcd->hid);
    if (lcd->claimed)
        libusb_release_interface(lcd->bulk, LCD_INTERFACE);
    if (lcd->bulk)
        libusb_close(lcd->bulk);
    if (lcd->usb)
        libusb_exit(lcd->usb);
    free(lcd->payload);
    free(lcd);
}

static int open_bulk_lcd(StreamLcd *lcd)
{
    if (libusb_init(&lcd->usb) != LIBUSB_SUCCESS)
        return 0;
    libusb_device **devices;
    ssize_t count = libusb_get_device_list(lcd->usb, &devices);
    if (count < 0)
        return 0;

    libusb_device *match = NULL;
    for (ssize_t i = 0; i < count; i++)
    {
        struct libusb_device_descriptor descriptor;
        if (libusb_get_device_descriptor(devices[i], &descriptor) != LIBUSB_SUCCESS ||
            descriptor.idVendor != lcd->profile->vendor_id)
            continue;
        if (descriptor.idProduct == NZXT_BOOTLOADER_PID ||
            (descriptor.idProduct == lcd->profile->product_id && match))
        {
            match = NULL;
            break;
        }
        if (descriptor.idProduct == lcd->profile->product_id)
            match = devices[i];
    }
    int result = match ? libusb_open(match, &lcd->bulk) : LIBUSB_ERROR_NOT_FOUND;
    libusb_free_device_list(devices, 1);
    if (result == LIBUSB_SUCCESS)
        result = libusb_claim_interface(lcd->bulk, LCD_INTERFACE);
    if (result != LIBUSB_SUCCESS)
    {
        log_message(LOG_ERROR, "LCD stream: cannot claim a unique %s: %s",
                    lcd->profile->name, libusb_error_name(result));
        return 0;
    }
    lcd->claimed = 1;
    return 1;
}

StreamLcd *lcd_stream_open(const DisplayProfile *profile)
{
    if (!profile ||
        (profile->preferred_transport != DISPLAY_TRANSPORT_STREAM_Q565 &&
         profile->preferred_transport != DISPLAY_TRANSPORT_STREAM_RGB565))
        return NULL;

    StreamLcd *lcd = calloc(1, sizeof(*lcd));
    if (!lcd)
        return NULL;
    lcd->profile = profile;
    lcd->capacity = profile->preferred_transport == DISPLAY_TRANSPORT_STREAM_RGB565
                        ? (size_t)profile->native_width * profile->native_height * 2
                        : LCD_MAX_PAYLOAD;
    lcd->payload = malloc(lcd->capacity);
    if (lcd->payload && open_bulk_lcd(lcd))
    {
        struct hid_device_info *devices =
            hid_enumerate(profile->vendor_id, profile->product_id);
        const char *path = NULL;
        for (const struct hid_device_info *it = devices; it; it = it->next)
        {
            if (it->interface_number != 1)
                continue;
            if (path)
            {
                path = NULL;
                break;
            }
            path = it->path;
        }
        if (path)
            lcd->hid = hid_open_path(path);
        hid_free_enumeration(devices);
        if (lcd->hid)
            return lcd;
        log_message(LOG_ERROR, "LCD stream: cannot open %s HID interface",
                    profile->name);
    }
    lcd_stream_close(lcd);
    return NULL;
}

static uint16_t rgb565(uint32_t argb)
{
    return (uint16_t)(((argb >> 8) & 0xf800U) |
                      ((argb >> 5) & 0x07e0U) | ((argb >> 3) & 0x001fU));
}

/* The Q565 literal and run ops are sufficient for complete frames.
 * Format reference: https://github.com/seritools/q565 */
static size_t encode_q565(cairo_surface_t *frame, uint8_t *output, size_t capacity)
{
    const unsigned char *pixels = cairo_image_surface_get_data(frame);
    const int stride = cairo_image_surface_get_stride(frame);
    const int width = cairo_image_surface_get_width(frame);
    const int height = cairo_image_surface_get_height(frame);
    memcpy(output, "q565", 4);
    output[4] = (uint8_t)width;
    output[5] = (uint8_t)(width >> 8);
    output[6] = (uint8_t)height;
    output[7] = (uint8_t)(height >> 8);
    size_t length = 8;
    unsigned run = 0;
    uint16_t previous = 0;
    for (int y = 0; y < height; y++)
    {
        const uint32_t *row = (const uint32_t *)(pixels + (size_t)y * stride);
        for (int x = 0; x < width; x++)
        {
            uint16_t pixel = rgb565(row[x]);
            if (pixel == previous)
                run++;
            if (run && (run == 62 || pixel != previous))
            {
                if (length == capacity)
                    return 0;
                output[length++] = (uint8_t)(0xc0U | (run - 1));
                run = 0;
            }
            if (pixel != previous)
            {
                if (capacity - length < 3)
                    return 0;
                output[length++] = 0xfe;
                output[length++] = (uint8_t)pixel;
                output[length++] = (uint8_t)(pixel >> 8);
                previous = pixel;
            }
        }
    }
    if (capacity - length < (run ? 2U : 1U))
        return 0;
    if (run)
        output[length++] = (uint8_t)(0xc0U | (run - 1));
    output[length++] = 0xff;
    return length;
}

static size_t encode_rgb565(cairo_surface_t *frame, uint8_t *output)
{
    const unsigned char *pixels = cairo_image_surface_get_data(frame);
    const int stride = cairo_image_surface_get_stride(frame);
    const int width = cairo_image_surface_get_width(frame);
    const int height = cairo_image_surface_get_height(frame);
    size_t length = 0;
    for (int y = 0; y < height; y++)
    {
        const uint32_t *row = (const uint32_t *)(pixels + (size_t)y * stride);
        for (int x = 0; x < width; x++)
        {
            const uint16_t pixel = rgb565(row[x]);
            output[length++] = (uint8_t)(pixel >> 8);
            output[length++] = (uint8_t)pixel;
        }
    }
    return length;
}

/* CAM capture of the LCD-only transaction:
 * https://github.com/kleju13/camless/blob/main/docs/protocol.md */
static int lcd_command(StreamLcd *lcd, uint8_t operation)
{
    uint8_t report[65] = {0, 0x36, operation};
    uint8_t reply[64];
    if (operation == 1)
    {
        report[4] = 1;
        report[5] = (uint8_t)lcd->profile->preferred_transport;
    }
    /* Discard stale reports. Keep the drain bounded on a busy HID interface. */
    for (int i = 0; i < 64; i++)
    {
        int length = hid_read_timeout(lcd->hid, reply, sizeof(reply), 0);
        if (length <= 0)
        {
            if (length < 0)
                return 0;
            break;
        }
    }
    if (hid_write(lcd->hid, report, sizeof(report)) != sizeof(report))
        return 0;

    const gint64 deadline = g_get_monotonic_time() + LCD_TIMEOUT_MS * 1000;
    gint64 remaining;
    while ((remaining = deadline - g_get_monotonic_time()) > 0)
    {
        int length = hid_read_timeout(lcd->hid, reply, sizeof(reply),
                                      (int)((remaining + 999) / 1000));
        if (length <= 0)
            return 0;
        if (length >= 16 && reply[0] == 0xff &&
            reply[14] == 0x36 && reply[15] == operation)
            return 0;
        if (length >= 15 && reply[0] == 0x37 && reply[1] == operation)
            return reply[14] == 1 ||
                   (lcd->profile->preferred_transport == DISPLAY_TRANSPORT_STREAM_RGB565 &&
                    reply[14] == 5);
    }
    return 0;
}

static int send_bulk(StreamLcd *lcd, const uint8_t *data, int size)
{
    int transferred = 0;
    return libusb_bulk_transfer(lcd->bulk, LCD_ENDPOINT, (uint8_t *)data,
                                size, &transferred, LCD_TIMEOUT_MS) == LIBUSB_SUCCESS &&
           transferred == size;
}

static int send_frame(StreamLcd *lcd, size_t size)
{
    uint8_t header[] = {
        0x12, 0xfa, 0x01, 0xe8, 0xab, 0xcd, 0xef, 0x98, 0x76, 0x54, 0x32, 0x10,
        (uint8_t)lcd->profile->preferred_transport, 0, 0, 0,
        (uint8_t)size, (uint8_t)(size >> 8),
        (uint8_t)(size >> 16), (uint8_t)(size >> 24)
    };
    if (lcd_command(lcd, 1) &&
        send_bulk(lcd, header, sizeof(header)) &&
        send_bulk(lcd, lcd->payload, (int)size) &&
        lcd_command(lcd, 2))
        return 1;

    /* End just this failed LCD transaction before releasing the interface. */
    lcd_command(lcd, 3);
    return 0;
}

int lcd_stream_send_frame(StreamLcd *lcd, cairo_surface_t *frame)
{
    if (!lcd || !frame || cairo_surface_status(frame) != CAIRO_STATUS_SUCCESS ||
        cairo_surface_get_type(frame) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(frame) != CAIRO_FORMAT_ARGB32 ||
        cairo_image_surface_get_width(frame) != lcd->profile->native_width ||
        cairo_image_surface_get_height(frame) != lcd->profile->native_height)
        return 0;

    cairo_surface_flush(frame);
    const int raw = lcd->profile->preferred_transport == DISPLAY_TRANSPORT_STREAM_RGB565;
    const size_t size = raw ? encode_rgb565(frame, lcd->payload)
                            : encode_q565(frame, lcd->payload, lcd->capacity);
    if (!size || size > LCD_MAX_PAYLOAD || !send_frame(lcd, size))
        return 0;

    /* Kraken 2023 firmware 2.x can need both buffers populated after init. */
    if (raw && !lcd->first_frame_sent && !send_frame(lcd, size))
        return 0;
    lcd->first_frame_sent = 1;
    return 1;
}
