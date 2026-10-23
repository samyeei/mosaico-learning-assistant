// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_config.h"
#include <ctype.h>
#include <string.h>
bool learning_config_valid(const learning_config_t *c)
{
    if (!c || c->version != 1 || c->region > 1 ||
        !memchr(c->ssid, 0, sizeof(c->ssid)) || !c->ssid[0] ||
        !memchr(c->password, 0, sizeof(c->password)) ||
        !memchr(c->key, 0, sizeof(c->key))) return false;
    size_t n = strlen(c->password);
    if (n && (n < 8 || n > 63)) return false;
    n = strlen(c->key);
    if (n < 8 || strncmp(c->key, "sk-", 3)) return false;
    for (size_t i = 0; i < n; ++i)
        if ((unsigned char)c->key[i] < 0x21 || (unsigned char)c->key[i] > 0x7e) return false;
    return true;
}
const char *learning_config_host(unsigned region)
{ return region == 1 ? "https://dashscope-intl.aliyuncs.com" : "https://dashscope.aliyuncs.com"; }
const char *learning_config_model(void) { return "qwen3-vl-plus"; }
