#ifndef COOLERDASH_STREAM_H
#define COOLERDASH_STREAM_H

#include "../device/config.h"
#include "../device/profile.h"

typedef struct
{
    struct _GdkPixbufAnimationIter *gif;
    const DisplayProfile *profile;
} StreamSource;

/** @brief Prepare a released LCD and decode its GIF without touching the active stream. */
StreamSource stream_prepare(const Config *config);
void stream_source_cleanup(StreamSource *source);
/** @brief Consume the prepared source and claim the LCD. */
void stream_start(const Config *config, StreamSource *source);
void stream_stop(void);
const DisplayProfile *stream_active_profile(void);
int stream_selected(void);
/** Draw when due and return the next monotonic deadline in microseconds. */
int64_t stream_draw(void);

#endif
