#ifndef SHUTDOWN_H
#define SHUTDOWN_H

#include <stddef.h>

/* The proxy accepts JSON only; images are transferred in bounded chunks. */
char *shutdown_image_action_token_json(void);
char *shutdown_image_import_json(const char *body, size_t length, int *http_status);
int shutdown_image_action_token_valid(const char *token);
void shutdown_image_import_cleanup(void);

#endif
