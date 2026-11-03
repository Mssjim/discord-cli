#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_TOKEN_LEN 256

bool config_get_token(char *token_out, size_t max_len);

#endif // CONFIG_H

