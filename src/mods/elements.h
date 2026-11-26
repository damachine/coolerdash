/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_ELEMENTS_H
#define COOLERDASH_ELEMENTS_H

#include <stdint.h>

#include "../srv/cc_sensor.h"

#define CONFIG_MAX_FONT_NAME_LEN 256
#define CONFIG_MAX_SENSOR_SLOT_LEN 256

/** @brief RGB color; is_set=0 uses default, is_set=1 uses custom value. */
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t is_set; // 0 = use default, 1 = user-defined (allows all RGB values)
} Color;

#define CUSTOM_LAYOUT_MAX_ELEMENTS 24
#define CUSTOM_LAYOUT_MAX_SCREENS 5
#define CUSTOM_SCREEN_DEFAULT_DURATION 6
#define CUSTOM_LAYOUT_NAME_SIZE 65
#define CUSTOM_LAYOUT_TEXT_SIZE 97
#define CUSTOM_LAYOUT_GRID 1000

typedef enum
{
    CUSTOM_VALUE,
    CUSTOM_TEXT,
    CUSTOM_CLOCK,
    CUSTOM_BAR,
    CUSTOM_RING,
    CUSTOM_FRAME
} CustomElementType;

int element_uses_font(CustomElementType type);
const char *element_log_name(CustomElementType type);
const char *layout_font_family(const char *font);

typedef enum
{
    CUSTOM_SOURCE_SENSOR,
    CUSTOM_SOURCE_CPU_LOAD,
    CUSTOM_SOURCE_GPU_LOAD,
    CUSTOM_SOURCE_CPU_POWER,
    CUSTOM_SOURCE_GPU_POWER,
    CUSTOM_SOURCE_POWER,
    CUSTOM_SOURCE_RAM_USED,
    CUSTOM_SOURCE_RAM_LOAD,
    CUSTOM_SOURCE_LIQUID_PUMP_DUTY,
    CUSTOM_SOURCE_LIQUID_PUMP_RPM
} CustomSource;

CustomSource get_custom_source(const char *source);

typedef enum
{
    CUSTOM_TEXT_ALIGN_AUTO,
    CUSTOM_TEXT_ALIGN_LEFT,
    CUSTOM_TEXT_ALIGN_CENTER,
    CUSTOM_TEXT_ALIGN_RIGHT
} CustomTextAlignment;

typedef struct
{
    CustomElementType type;
    int x, y, width, height;
    int column, grow;
    int font_size, segments;
    CustomTextAlignment align;
    sensor_category_t channel;
    int ring_thickness, ring_start, ring_sweep;
    double minimum, maximum;
    Color color;
    char font_face[CONFIG_MAX_FONT_NAME_LEN];
    char text[CUSTOM_LAYOUT_TEXT_SIZE];
    char source[CONFIG_MAX_SENSOR_SLOT_LEN];
} CustomElement;

typedef enum
{
    CUSTOM_FREE,
    CUSTOM_COLUMNS
} CustomArrangement;

typedef struct
{
    int count;
    int duration;
    CustomArrangement arrangement;
    CustomElement elements[CUSTOM_LAYOUT_MAX_ELEMENTS];
} CustomScreen;

typedef struct
{
    int screen_count;
    int needs_sensors;
    int fit_clock;
    CustomScreen screens[CUSTOM_LAYOUT_MAX_SCREENS];
} CustomLayout;

struct json_t;
int load_custom_layout(struct json_t *item, CustomLayout *layout, Color *background);
int load_layout_preset(const char *name, CustomLayout *layout);
extern const char layout_presets_json[];
int load_active_custom_layout(struct json_t *root, CustomLayout *layout, Color *background);

#endif
