// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "app_orientation.h"

#include "app_orientation_logic.h"
#include "bsp/esp_mosaico.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdatomic.h>

#define ORIENTATION_SAMPLE_MS 50U
#define ORIENTATION_STARTUP_MS 100U
#define ORIENTATION_PAUSE_TIMEOUT_MS 1000U
#define ORIENTATION_TASK_STACK 4096U
#define ORIENTATION_TASK_PRIORITY 4U
#define ACCEL_LOW_PASS_ALPHA 0.25f

static const char *TAG = "orientation";
static TaskHandle_t s_task;
// App-lifetime singleton, initialized before camera/runtime tasks start.
static SemaphoreHandle_t s_frame_lock;
static atomic_uint s_rotation;
static atomic_uint s_mode=1;
void app_orientation_set_mode(bool automatic,unsigned fixed_rotation)
{if(fixed_rotation<=270&&fixed_rotation%90==0)atomic_store(&s_mode,(automatic?1U:0U)|((fixed_rotation/90)<<1));}
void app_orientation_frame_lock(void){if(s_frame_lock)xSemaphoreTake(s_frame_lock,portMAX_DELAY);}
void app_orientation_frame_unlock(void){if(s_frame_lock)xSemaphoreGive(s_frame_lock);}
unsigned app_orientation_get(void){return atomic_load(&s_rotation);}

static esp_err_t apply_rotation(esp_gsp_handle_t *ui,
                                app_orientation_t orientation,unsigned mode)
{
    esp_gsp_esp_lcd_pause_t *pause = NULL;
    app_orientation_frame_lock();
    if(mode!=atomic_load(&s_mode)){app_orientation_frame_unlock();return ESP_ERR_INVALID_STATE;}
    esp_err_t pause_err=esp_gsp_esp_lcd_pause(*ui, ORIENTATION_PAUSE_TIMEOUT_MS, &pause);
    if(pause_err!=ESP_OK){app_orientation_frame_unlock();return pause_err;}

    const esp_err_t rotate_err = mode==atomic_load(&s_mode)?
        bsp_display_set_rotation((bsp_display_rotation_t)orientation):ESP_ERR_INVALID_STATE;
    atomic_store(&s_rotation,(unsigned)bsp_display_get_rotation());

    esp_gsp_handle_t resumed_ui = NULL;
    esp_err_t resume_err =
        esp_gsp_esp_lcd_resume_paused(pause, &resumed_ui);
    // Keep the unconsumed pause token and ownership boundary on failure.
    // Retry with backoff instead of losing the token and leaving a paused UI.
    unsigned retries=0;
    while (resume_err != ESP_OK) {
        if(retries++%20==0)ESP_LOGW(TAG, "resume after rotation failed, retrying: %s",esp_err_to_name(resume_err));
        vTaskDelay(pdMS_TO_TICKS(ORIENTATION_SAMPLE_MS));
        resume_err = esp_gsp_esp_lcd_resume_paused(pause, &resumed_ui);
    }
    if (resume_err == ESP_OK && resumed_ui != NULL) {
        *ui = resumed_ui;
    }
    app_orientation_frame_unlock();
    ESP_RETURN_ON_ERROR(rotate_err, TAG, "rotate display and touch");
    return ESP_OK;
}

static void orientation_task(void *arg)
{
    esp_gsp_handle_t ui = arg;
    app_orientation_filter_t filter;
    app_orientation_filter_init(
        &filter, (app_orientation_t)bsp_display_get_rotation());

    vTaskDelay(pdMS_TO_TICKS(ORIENTATION_STARTUP_MS));
    bool have_filtered_sample = false;
    float filtered_x = 0.0f;
    float filtered_y = 0.0f;
    float filtered_z = 0.0f;

    while (true) {
        unsigned mode=atomic_load(&s_mode);
        if(!(mode&1U)){
            unsigned fixed=(mode>>1)*90;
            if(app_orientation_get()!=fixed)(void)apply_rotation(&ui,(app_orientation_t)fixed,mode);
            app_orientation_filter_init(&filter,(app_orientation_t)app_orientation_get());
            have_filtered_sample=false;
            vTaskDelay(pdMS_TO_TICKS(ORIENTATION_SAMPLE_MS));continue;
        }
        float x_g = 0.0f;
        float y_g = 0.0f;
        float z_g = 0.0f;
        if (bsp_imu_get_accel(&x_g, &y_g, &z_g) == ESP_OK) {
            if (!have_filtered_sample) {
                filtered_x = x_g;
                filtered_y = y_g;
                filtered_z = z_g;
                have_filtered_sample = true;
            } else {
                filtered_x += (x_g - filtered_x) * ACCEL_LOW_PASS_ALPHA;
                filtered_y += (y_g - filtered_y) * ACCEL_LOW_PASS_ALPHA;
                filtered_z += (z_g - filtered_z) * ACCEL_LOW_PASS_ALPHA;
            }

            app_orientation_t requested;
            if (app_orientation_filter_observe(
                    &filter, filtered_x, filtered_y, filtered_z, &requested)) {
                const esp_err_t err = apply_rotation(&ui, requested,mode);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "gravity rotation applied: %d degrees accel=%+.2f,%+.2f,%+.2f",
                             (int)requested,filtered_x,filtered_y,filtered_z);
                } else {
                    ESP_LOGE(TAG, "gravity rotation failed: %s",
                             esp_err_to_name(err));
                    app_orientation_filter_init(
                        &filter,
                        (app_orientation_t)bsp_display_get_rotation());
                }
            }
        }
        // No catch-up burst after a slow capture or display fence: stability
        // samples must be separated in real time, not counted in a tight loop.
        vTaskDelay(pdMS_TO_TICKS(ORIENTATION_SAMPLE_MS));
    }
}

esp_err_t app_orientation_start(esp_gsp_handle_t ui)
{
    if (ui == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_frame_lock=xSemaphoreCreateMutex();
    if(!s_frame_lock)return ESP_ERR_NO_MEM;
    atomic_init(&s_rotation,(unsigned)bsp_display_get_rotation());

    const bsp_imu_config_t config = BSP_IMU_CONFIG_DEFAULT();
    esp_err_t init_err=bsp_imu_init();
    if(init_err!=ESP_OK){vSemaphoreDelete(s_frame_lock);s_frame_lock=NULL;return init_err;}
    const esp_err_t start_err = bsp_imu_start(&config);
    if (start_err != ESP_OK) {
        (void)bsp_imu_deinit();
        vSemaphoreDelete(s_frame_lock);s_frame_lock=NULL;
        ESP_RETURN_ON_ERROR(start_err, TAG, "start BMI270");
    }

    if (xTaskCreate(orientation_task, "orientation", ORIENTATION_TASK_STACK,
                    ui, ORIENTATION_TASK_PRIORITY, &s_task) != pdPASS) {
        (void)bsp_imu_stop();
        (void)bsp_imu_deinit();
        vSemaphoreDelete(s_frame_lock);s_frame_lock=NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "gravity auto-rotation started (0/90/180/270 degrees)");
    return ESP_OK;
}
