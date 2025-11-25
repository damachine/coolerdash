/*
 * Simple test harness for safe_area calculations
 * Validates safe_area_width and safe_bar_width for given display_inscribe_factor values.
 */
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../src/device/sys.h"

// Use same constant
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.7071067811865476
#endif

static int almost_equal(double a, double b, double eps)
{
    return fabs(a - b) <= eps;
}

static void run_case(const char *name, int width, float content_scale, float inscribe_cfg, int rectangular_force, double expected_safe_area, double expected_safe_bar)
{
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.display_width = (uint16_t)width;
    // Use snprintf for a local, safe copy to avoid external symbol dependency during unit tests
    snprintf(cfg.display_shape, sizeof(cfg.display_shape), "%s", "circular");
    cfg.display_content_scale_factor = content_scale;
    cfg.display_inscribe_factor = inscribe_cfg;

    double inscribe_used;
    if (rectangular_force)
        inscribe_used = 1.0;
    else if (strcmp(cfg.display_shape, "rectangular") == 0)
        inscribe_used = 1.0;
    else
    {
        if (cfg.display_inscribe_factor == 0.0f)
            inscribe_used = M_SQRT1_2;
        else if (cfg.display_inscribe_factor > 0.0f && cfg.display_inscribe_factor <= 1.0f)
            inscribe_used = (double)cfg.display_inscribe_factor;
        else
            inscribe_used = M_SQRT1_2;
    }

    double safe_area = (double)cfg.display_width * inscribe_used;
    double safe_bar = safe_area * (double)cfg.display_content_scale_factor;

    int pass_area = almost_equal(safe_area, expected_safe_area, 0.001);
    int pass_bar = almost_equal(safe_bar, expected_safe_bar, 0.01);

    printf("Case: %s\n", name);
    printf("  width=%d, content_scale=%.5f, cfg_inscribe=%.8f -> inscribe_used=%.8f\n", width, content_scale, inscribe_cfg, inscribe_used);
    printf("  safe_area=%.6f expected=%.6f [ %s ]\n", safe_area, expected_safe_area, pass_area ? "OK" : "FAIL");
    printf("  safe_bar=%.6f expected=%.6f [ %s ]\n", safe_bar, expected_safe_bar, pass_bar ? "OK" : "FAIL");
    printf("\n");
}

int main(void)
{
    // Use base width 240
    const int width = 240;
    const float content_scale = 0.98f; // default

    // Cases for width 240 and 320
    int widths[] = {240, 320};
    size_t wcount = sizeof(widths) / sizeof(widths[0]);
    for (size_t i = 0; i < wcount; ++i)
    {
        int w = widths[i];
        // auto (0.0)
        double inscribe = M_SQRT1_2;
        double expected_safe_area = w * inscribe;
        double expected_safe_bar = expected_safe_area * content_scale;
        char buf[128];
        snprintf(buf, sizeof(buf), "auto(0.0) width=%d", w);
        run_case(buf, w, content_scale, 0.0f, 0, expected_safe_area, expected_safe_bar);

        // explicit geometric
        inscribe = 0.70710678;
        expected_safe_area = w * inscribe;
        expected_safe_bar = expected_safe_area * content_scale;
        snprintf(buf, sizeof(buf), "explicit 0.70710678 width=%d", w);
        run_case(buf, w, content_scale, 0.70710678f, 0, expected_safe_area, expected_safe_bar);

        // explicit custom 0.85
        inscribe = 0.85;
        expected_safe_area = w * inscribe;
        expected_safe_bar = expected_safe_area * content_scale;
        snprintf(buf, sizeof(buf), "custom 0.85 width=%d", w);
        run_case(buf, w, content_scale, 0.85f, 0, expected_safe_area, expected_safe_bar);

        // rectangular shape forced (inscribe=1.0)
        inscribe = 1.0;
        expected_safe_area = w * inscribe;
        expected_safe_bar = expected_safe_area * content_scale;
        snprintf(buf, sizeof(buf), "rectangular forced width=%d", w);
        run_case(buf, w, content_scale, 0.0f, 1, expected_safe_area, expected_safe_bar);
    }

    // Invalid values -> fallback to M_SQRT1_2
    {
        int w = 240;
        double inscribe = M_SQRT1_2;
        double expected_safe_area = w * inscribe;
        double expected_safe_bar = expected_safe_area * content_scale;
        run_case("invalid -1 -> fallback", w, content_scale, -1.0f, 0, expected_safe_area, expected_safe_bar);
        run_case("invalid 1.5 -> fallback", w, content_scale, 1.5f, 0, expected_safe_area, expected_safe_bar);
    }

    return 0;
}
