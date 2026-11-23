#ifndef COOLERDASH_LCD_H
#define COOLERDASH_LCD_H

#include <cairo/cairo.h>
#include "profile.h"

typedef struct StreamLcd StreamLcd;

StreamLcd *lcd_stream_open(const DisplayProfile *profile);
void lcd_stream_close(StreamLcd *lcd);
int lcd_stream_send_frame(StreamLcd *lcd, cairo_surface_t *frame);

#endif
