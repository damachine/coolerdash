#ifndef IMAGES_H
#define IMAGES_H

#include <stddef.h>

/* The proxy accepts JSON only; images are transferred in bounded chunks. */
char *image_action_token_json(void);
char *shutdown_image_import_json(const char *body, size_t length, int *http_status);
char *background_image_import_json(const char *body, size_t length, int *http_status);
int image_action_token_valid(const char *token);
void image_import_cleanup(void);

#endif
