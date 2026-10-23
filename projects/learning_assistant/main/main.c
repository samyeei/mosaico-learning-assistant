// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "board_display.h"
#include "app_orientation.h"
#include "learning_controls_device.h"
#include "bsp/esp_mosaico.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "learning_ui.h"
#include "learning_network.h"
#include "learning_runtime.h"
#include "iris_screen_mirror.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"
#include "ui_bundle.h"

static const char *TAG = "learning_assistant";
enum { LEARNING_FONT_RENDER_STACK_BYTES = 64 * 1024 };
static learning_ui_handle_t s_learning;
extern const unsigned char learning_font_start[] asm("_binary_LearningSans_ttf_start");
extern const unsigned char learning_font_end[] asm("_binary_LearningSans_ttf_end");

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Keep Recovery reachable even when the external UI image is absent or
     * invalid. */
    iris_ota_support_start();

    esp_gsp_config_t app_config;
    const esp_err_t bundle_err = ui_bundle_open(&app_config);
    if (bundle_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "GSP UI unavailable (0x%x); ESP-Iris Recovery RPC remains active",
                 bundle_err);
        return;
    }
    app_config.ttf = learning_font_start;
    app_config.ttf_size = learning_font_end - learning_font_start;
    // Camera preview uses a bounded RGB565 Canvas pool, not encoded images.
    // Static lossless assets can use region decoding without a decoded cache.
    ESP_ERROR_CHECK(esp_gsp_config_set(&app_config,
        ESP_GSP_FIELD_ENABLE_IMAGE_CACHE,0)==ESP_GSP_CONFIG_SET_OK?ESP_OK:ESP_ERR_INVALID_STATE);

    // Drawer/scene snapshots retain two full-screen RGB565 buffers (~900 KiB).
    // Keep the documented live transition path so DVP can own its two 720p frames
    // even after repeatedly opening settings. Native drawer gestures still work.
    ESP_ERROR_CHECK(esp_gsp_config_set(&app_config,
        ESP_GSP_FIELD_ENABLE_TRANSITION_SNAPSHOTS,0)==ESP_GSP_CONFIG_SET_OK?ESP_OK:ESP_ERR_INVALID_STATE);

    esp_display_present_target_config_t display;
    ESP_ERROR_CHECK(board_display_init(&display));

    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(board_touch_init(&touch));

    esp_gsp_esp_lcd_config_t lcd = ESP_GSP_ESP_LCD_CONFIG_INIT();
    lcd.display = display;
    lcd.touch = touch;
    /* The archived first-device core shows FreeType's gray rasterizer's
     * 16 KiB local pool overflowing the default render-task stack. UI callbacks
     * only enqueue work; NVS/network operations stay on internal worker stacks,
     * so a 64 KiB PSRAM render stack is safe (also used by muse_companion). */
    lcd.task_stack_psram = true;
    lcd.task_stack_size_freetype = LEARNING_FONT_RENDER_STACK_BYTES;

    ESP_ERROR_CHECK(iris_screen_mirror_init());

    esp_gsp_handle_t ui;
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&app_config, &lcd, &ui));
    ESP_ERROR_CHECK(iris_screen_mirror_attach(ui));
    ESP_ERROR_CHECK_WITHOUT_ABORT(app_orientation_start(ui));
    ESP_ERROR_CHECK(learning_controls_start(ui));
    ESP_ERROR_CHECK_WITHOUT_ABORT(learning_network_start());
    ESP_ERROR_CHECK(learning_runtime_start(ui));
    ESP_ERROR_CHECK(learning_ui_create(ui, &s_learning));

    ESP_LOGI(TAG, "Learning assistant ready at %dx%d RGB565",
             BSP_LCD_H_RES, BSP_LCD_V_RES);
}
