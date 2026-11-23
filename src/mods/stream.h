#ifndef COOLERDASH_STREAM_H
#define COOLERDASH_STREAM_H

#include "../device/config.h"
#include "../device/profile.h"

void stream_start(const Config *config);
void stream_stop(void);
const DisplayProfile *stream_active_profile(void);
int stream_selected(void);
/** Draw when due and return the next monotonic deadline in microseconds. */
int64_t stream_draw(void);

#endif
