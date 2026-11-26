/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "elements.h"
#include "../device/config.h"
#include "../srv/cc_conf.h"

enum
{
    ELEMENT_FONT = 1,
    ELEMENT_SOURCE = 2,
    ELEMENT_RANGE = 4
};

static const struct
{
    const char *type;
    const char *log_name;
    unsigned fields;
} element_types[] = {
    [CUSTOM_VALUE] = {"value", "value", ELEMENT_FONT | ELEMENT_SOURCE},
    [CUSTOM_TEXT] = {"text", "label", ELEMENT_FONT},
    [CUSTOM_CLOCK] = {"clock", "clock", ELEMENT_FONT},
    [CUSTOM_BAR] = {"bar", "bar", ELEMENT_SOURCE | ELEMENT_RANGE},
    [CUSTOM_RING] = {"ring", "ring", ELEMENT_SOURCE | ELEMENT_RANGE},
    [CUSTOM_FRAME] = {"frame", "frame", 0},
};

int element_uses_font(CustomElementType type)
{
    return (element_types[type].fields & ELEMENT_FONT) != 0;
}

const char *element_log_name(CustomElementType type)
{
    return element_types[type].log_name;
}

static int custom_integer(json_t *object, const char *key, int min, int max,
                          int *result)
{
    json_t *field = json_object_get(object, key);
    json_int_t value = json_integer_value(field);
    if (!json_is_integer(field) || value < min || value > max)
        return 0;
    *result = (int)value;
    return 1;
}

static int custom_optional_integer(json_t *object, const char *key, int min, int max, int *result)
{
    return !json_object_get(object, key) || custom_integer(object, key, min, max, result);
}

static int custom_color(json_t *object, const char *key, Color *result)
{
    const char *value = config_json_string(object, key, 8);
    if (!value || strlen(value) != 7 || value[0] != '#' ||
        strspn(value + 1, "0123456789abcdefABCDEF") != 6)
        return 0;
    unsigned long rgb = strtoul(value + 1, NULL, 16);
    *result = (Color){(uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb, 1};
    return 1;
}

CustomSource get_custom_source(const char *source)
{
    static const char *names[] = {
        [CUSTOM_SOURCE_CPU_LOAD] = "cpu-load",
        [CUSTOM_SOURCE_GPU_LOAD] = "gpu-load",
        [CUSTOM_SOURCE_CPU_POWER] = "cpu-power",
        [CUSTOM_SOURCE_GPU_POWER] = "gpu-power",
        [CUSTOM_SOURCE_POWER] = "power",
        [CUSTOM_SOURCE_RAM_USED] = "ram-used",
        [CUSTOM_SOURCE_RAM_LOAD] = "ram-load",
        [CUSTOM_SOURCE_LIQUID_PUMP_DUTY] = "liquid-pump-duty",
        [CUSTOM_SOURCE_LIQUID_PUMP_RPM] = "liquid-pump-rpm",
    };
    for (size_t i = 1; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(source, names[i]) == 0)
            return (CustomSource)i;
    return CUSTOM_SOURCE_SENSOR;
}

const char *layout_font_family(const char *font)
{
    if (strcmp(font, "sans") == 0)
        return "sans-serif";
    if (strcmp(font, "mono") == 0)
        return "monospace";
    return font;
}

static int load_custom_element(json_t *object, CustomElement *element)
{
    const char *type = json_string_value(json_object_get(object, "type"));
    const size_t type_count = sizeof(element_types) / sizeof(element_types[0]);
    size_t index = 0;
    while (index < type_count && (!type || strcmp(type, element_types[index].type) != 0))
        index++;
    if (index == type_count ||
        !custom_integer(object, "x", 0, CUSTOM_LAYOUT_GRID, &element->x) ||
        !custom_integer(object, "y", 0, CUSTOM_LAYOUT_GRID, &element->y) ||
        !custom_integer(object, "w", 20, CUSTOM_LAYOUT_GRID - element->x,
                        &element->width) ||
        !custom_integer(object, "h", 20, CUSTOM_LAYOUT_GRID - element->y,
                        &element->height) ||
        !custom_color(object, "color", &element->color))
        return 0;
    element->type = (CustomElementType)index;
    element->channel = SENSOR_CATEGORY_TEMP;
    const json_t *grow = json_object_get(object, "grow");
    element->column = 0;
    if (!custom_optional_integer(object, "column", -1, 1, &element->column) ||
        (element->column < 0 && element->type != CUSTOM_FRAME) ||
        (grow && !json_is_boolean(grow)))
        return 0;
    element->grow = json_is_true(grow);

    if (element_uses_font(element->type))
    {
        const char *font = config_json_string(object, "font", sizeof(element->font_face));
        if (!font || font[strspn(font, " ")] == '\0' ||
            !custom_integer(object, "size", 8, 120, &element->font_size))
            return 0;
        SAFE_STRCPY(element->font_face, layout_font_family(font));
        if (element->type != CUSTOM_CLOCK)
        {
            const char *label = config_json_string(object, "text", sizeof(element->text));
            if (!label)
                return 0;
            SAFE_STRCPY(element->text, label);
        }
    }

    if (element_types[index].fields & ELEMENT_SOURCE)
    {
        const char *source = config_json_string(object, "source", sizeof(element->source));
        const char *separator = source ? strchr(source, ':') : NULL;
        if (!source || (*source && !is_legacy_sensor_slot(source) &&
                        get_custom_source(source) == CUSTOM_SOURCE_SENSOR &&
                        (!separator || separator == source || !separator[1])) ||
            strcmp(source, "none") == 0)
            return 0;
        SAFE_STRCPY(element->source, source);

        json_t *channel = json_object_get(object, "channel");
        if (channel)
        {
            const char *name = json_string_value(channel);
            if (!name || get_custom_source(source) != CUSTOM_SOURCE_SENSOR)
                return 0;
            if (strcmp(name, "duty") == 0)
                element->channel = SENSOR_CATEGORY_DUTY;
            else if (strcmp(name, "watts") == 0)
                element->channel = SENSOR_CATEGORY_WATTS;
            else if (strcmp(name, "rpm") == 0)
                element->channel = SENSOR_CATEGORY_RPM;
            else if (strcmp(name, "freq") == 0)
                element->channel = SENSOR_CATEGORY_FREQ;
            else
                return 0;
        }
    }

    if (element_types[index].fields & ELEMENT_RANGE)
    {
        json_t *min = json_object_get(object, "min");
        json_t *max = json_object_get(object, "max");
        element->minimum = json_number_value(min);
        element->maximum = json_number_value(max);
        if (!json_is_number(min) || !json_is_number(max) ||
            element->minimum < -1000000 || element->maximum > 1000000 ||
            element->minimum >= element->maximum)
            return 0;
    }
    if (element->type == CUSTOM_BAR)
        return custom_integer(object, "segments", 1, 24, &element->segments);
    if (element->type == CUSTOM_RING)
        return custom_optional_integer(object, "segments", 1, 24, &element->segments) &&
               custom_optional_integer(object, "ring_thickness", 0, 120, &element->ring_thickness) &&
               custom_optional_integer(object, "ring_start", 0, 359, &element->ring_start) &&
               custom_optional_integer(object, "ring_sweep", 1, 360, &element->ring_sweep);
    return 1;
}

static int load_custom_screen(json_t *object, CustomScreen *screen, int *needs_sensors,
                              int default_duration)
{
    json_t *elements = json_object_get(object, "elements");
    json_t *arrangement = json_object_get(object, "arrangement");
    screen->duration = default_duration;
    screen->arrangement = CUSTOM_FREE;
    if (arrangement)
    {
        const char *name = json_string_value(arrangement);
        if (name && strcmp(name, "columns") == 0)
            screen->arrangement = CUSTOM_COLUMNS;
        else if (!name || strcmp(name, "free") != 0)
            return 0;
    }
    if (!json_is_array(elements) || json_array_size(elements) > CUSTOM_LAYOUT_MAX_ELEMENTS ||
        !custom_optional_integer(object, "duration", 1, 60, &screen->duration))
        return 0;

    size_t index;
    json_t *candidate;
    json_array_foreach(elements, index, candidate)
    {
        CustomElement *element = &screen->elements[index];
        if (!load_custom_element(candidate, element))
            return 0;
        const CustomSource source = get_custom_source(element->source);
        if (element->source[0] && source != CUSTOM_SOURCE_RAM_USED && source != CUSTOM_SOURCE_RAM_LOAD)
            *needs_sensors = 1;
    }
    screen->count = (int)json_array_size(elements);
    return 1;
}

int load_custom_layout(json_t *item, CustomLayout *layout, Color *background)
{
    int version;
    Color color;
    const char *name = config_json_string(item, "name", CUSTOM_LAYOUT_NAME_SIZE);
    const json_t *fit_clock = json_object_get(item, "fit_clock");
    if (!name || name[strspn(name, " ")] == '\0' ||
        !custom_integer(item, "version", 1, 2, &version) ||
        !custom_color(item, "background", &color) ||
        (fit_clock && !json_is_boolean(fit_clock)))
        return 0;

    if (version == 1)
    {
        if (!load_custom_screen(item, &layout->screens[0], &layout->needs_sensors,
                                CUSTOM_SCREEN_DEFAULT_DURATION))
            return 0;
        layout->screen_count = 1;
    }
    else
    {
        json_t *screens = json_object_get(item, "screens");
        const size_t count = json_array_size(screens);
        int default_duration = CUSTOM_SCREEN_DEFAULT_DURATION;
        if (!count || count > CUSTOM_LAYOUT_MAX_SCREENS ||
            !custom_optional_integer(item, "switch_interval", 1, 60, &default_duration))
            return 0;
        size_t index;
        json_t *candidate;
        json_array_foreach(screens, index, candidate)
        {
            if (!load_custom_screen(candidate, &layout->screens[index], &layout->needs_sensors,
                                    default_duration))
                return 0;
        }
        layout->screen_count = (int)count;
    }
    layout->fit_clock = json_is_true(fit_clock);
    if (background)
        *background = color;
    return 1;
}

int load_layout_preset(const char *name, CustomLayout *layout)
{
    json_t *presets = json_loads(layout_presets_json, JSON_REJECT_DUPLICATES, NULL);
    const int loaded = load_custom_layout(json_object_get(presets, name), layout, NULL);
    json_decref(presets);
    if (!loaded)
        log_message(LOG_ERROR, "Invalid built-in layout preset: %s", name);
    return loaded;
}

int load_active_custom_layout(json_t *root, CustomLayout *layout, Color *background)
{
    json_t *bank = json_object_get(root, "custom_layouts");
    const char *active = config_json_string(bank, "active_id", 49);
    json_t *items = json_object_get(bank, "items");
    if (!active || !*active || !json_is_array(items))
        return 0;

    json_t *item = NULL;
    json_t *candidate;
    size_t index;
    json_array_foreach(items, index, candidate)
    {
        const char *id = config_json_string(candidate, "id", 49);
        if (id && strcmp(id, active) == 0)
        {
            if (item)
                return 0;
            item = candidate;
        }
    }

    return load_custom_layout(item, layout, background);
}
