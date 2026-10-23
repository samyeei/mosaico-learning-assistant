#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct fake_http_t *esp_http_client_handle_t;
typedef struct { const char *url;int timeout_ms;void *crt_bundle_attach;int buffer_size;
    bool disable_auto_redirect,keep_alive_enable; } esp_http_client_config_t;
enum { HTTP_METHOD_POST };
#define ESP_ERR_HTTP_EAGAIN 0x7007
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_set_method(esp_http_client_handle_t client,int method);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client,const char *key,const char *value);
esp_err_t esp_http_client_open(esp_http_client_handle_t client,int size);
int esp_http_client_write(esp_http_client_handle_t client,const char *data,int size);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client,char *data,int size);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t client,int *code,int *flags);
