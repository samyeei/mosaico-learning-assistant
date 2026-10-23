#pragma once
#define ESP_LOGI(tag,...) ((void)0)
// Hardware transport diagnostics are not emitted by isolated protocol tests.
#define ESP_LOGW(tag,...) ((void)(tag))
