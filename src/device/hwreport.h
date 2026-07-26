/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

#ifndef HWREPORT_H
#define HWREPORT_H

#include <stddef.h>
#include <sys/types.h>

#include "config.h"

#define HARDWARE_REPORT_PATH_SIZE 1024

typedef struct HardwareReportOptions
{
    const char *output_dir;
    const char *device_uid;
    int test_lcd;
} HardwareReportOptions;

/** Resolve the default report directory to the invoking user's home. */
int hardware_report_default_output_dir(char *buffer, size_t buffer_size);

#ifdef HARDWARE_REPORT_TESTING
/** Exercise regular and sudo home resolution without changing process IDs. */
int hardware_report_default_output_dir_for_test(
    char *buffer, size_t buffer_size, uid_t real_uid, uid_t effective_uid,
    const char *sudo_uid);
#endif

/** Collect, sanitize, and write the JSON and Markdown hardware reports. */
int run_hardware_report(const Config *config,
                        const HardwareReportOptions *options,
                        const char *coolerdash_version);

#endif // HWREPORT_H
