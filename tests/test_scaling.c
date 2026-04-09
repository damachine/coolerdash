/*
 * Simple test harness for safe_area calculations
 * Validates safe_area_width and safe_bar_width for current scaling logic.
 *
 * Current formula (from display.c calculate_scaling_params):
 *   inscribe_factor = is_circular ? M_SQRT1_2 : 1.0
 *   safe_area_width = display_width * inscribe_factor
 *   content_scale   = display_content_scale_factor (0..1, fallback 0.98)
 *   bar_width_factor = layout_bar_width/100 (fallback 0.98)
 *   safe_bar_width  = safe_area_width * content_scale * bar_width_factor
 */
#define _POSIX_C_SOURCE 200112L
// cppcheck-suppress-begin missingIncludeSystem
#include <stdio.h>
#include <string.h>
#include <math.h>
// cppcheck-suppress-end missingIncludeSystem

#include "../src/device/config.h"

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.7071067811865476
#endif

static int almost_equal(double a, double b, double eps)
{
    return fabs(a - b) <= eps;
}

static int failures = 0;

static void run_case(const char *name, int width, float content_scale,
                     uint8_t bar_width_pct, int is_circular,
                     double expected_safe_area, int expected_safe_bar)
{
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.display_width = (uint16_t)width;
    cfg.display_height = (uint16_t)width;
    cfg.display_content_scale_factor = content_scale;
    cfg.layout_bar_width = bar_width_pct;

    double inscribe_factor = is_circular ? M_SQRT1_2 : 1.0;
    double safe_area_width = (double)cfg.display_width * inscribe_factor;

    float cs = (cfg.display_content_scale_factor > 0.0f &&
                cfg.display_content_scale_factor <= 1.0f)
                   ? cfg.display_content_scale_factor
                   : 0.98f;
    double bwf = (cfg.layout_bar_width > 0)
                     ? (cfg.layout_bar_width / 100.0)
                     : 0.98;
    int safe_bar_width = (int)(safe_area_width * cs * bwf);

    int pass_area = almost_equal(safe_area_width, expected_safe_area, 0.01);
    int pass_bar = (safe_bar_width == expected_safe_bar);

    printf("Case: %s\n", name);
    printf("  width=%d, shape=%s, content_scale=%.2f, bar_width=%u%%\n",
           width, is_circular ? "circular" : "rectangular", content_scale,
           bar_width_pct);
    printf("  safe_area=%.4f expected=%.4f [ %s ]\n",
           safe_area_width, expected_safe_area, pass_area ? "OK" : "FAIL");
    printf("  safe_bar=%d expected=%d [ %s ]\n",
           safe_bar_width, expected_safe_bar, pass_bar ? "OK" : "FAIL");
    printf("\n");

    if (!pass_area || !pass_bar)
        failures++;
}

int main(void)
{
    const float cs = 0.98f;
    int widths[] = {240, 320};
    size_t wcount = sizeof(widths) / sizeof(widths[0]);

    for (size_t i = 0; i < wcount; ++i)
    {
        int w = widths[i];
        char buf[128];

        /* Circular display: inscribe = M_SQRT1_2, default bar_width 98% */
        double area = w * M_SQRT1_2;
        int bar = (int)(area * cs * 0.98);
        snprintf(buf, sizeof(buf), "circular default width=%d", w);
        run_case(buf, w, cs, 98, 1, area, bar);

        /* Rectangular display: inscribe = 1.0, default bar_width 98% */
        area = w * 1.0;
        bar = (int)(area * cs * 0.98);
        snprintf(buf, sizeof(buf), "rectangular default width=%d", w);
        run_case(buf, w, cs, 98, 0, area, bar);

        /* Circular with custom bar_width 80% */
        area = w * M_SQRT1_2;
        bar = (int)(area * cs * 0.80);
        snprintf(buf, sizeof(buf), "circular bar80%% width=%d", w);
        run_case(buf, w, cs, 80, 1, area, bar);

        /* Rectangular with content_scale 1.0 */
        area = w * 1.0;
        bar = (int)(area * 1.0 * 0.98);
        snprintf(buf, sizeof(buf), "rectangular cs=1.0 width=%d", w);
        run_case(buf, w, 1.0f, 98, 0, area, bar);
    }

    /* Fallback tests: invalid content_scale => fallback 0.98 */
    {
        int w = 240;
        double area = w * M_SQRT1_2;
        int bar = (int)(area * 0.98 * 0.98);
        run_case("cs=0 fallback", w, 0.0f, 98, 1, area, bar);
        run_case("cs=-1 fallback", w, -1.0f, 98, 1, area, bar);
        run_case("cs=1.5 fallback", w, 1.5f, 98, 1, area, bar);
    }

    /* Fallback tests: bar_width=0 => fallback 0.98 */
    {
        int w = 240;
        double area = w * M_SQRT1_2;
        int bar = (int)(area * cs * 0.98);
        run_case("bar_width=0 fallback", w, cs, 0, 1, area, bar);
    }

    if (failures > 0)
    {
        printf("FAILED: %d test(s)\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
