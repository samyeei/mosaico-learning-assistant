#pragma once
#include <stddef.h>
int mbedtls_base64_encode(unsigned char *out,size_t capacity,size_t *written,
                         const unsigned char *data,size_t size);
