idf_component_register(
    SRCS "main.c" "learning_ui.c" "learning_model.c" "learning_config.c"
        "learning_network.c" "learning_provider.c" "learning_runtime.c" "learning_camera.c" "board_display.c"
        "app_orientation.c" "app_orientation_logic.c" "learning_viewport.c"
        "learning_speech.c" "learning_conversation.c"
        "learning_controls.c" "learning_controls_device.c"
    INCLUDE_DIRS "."
    REQUIRES esp_iris esp-mosaico-bsp esp_mosaico_app_recovery
        esp_mosaico_gsp_bundle esp_mosaico_gsp_iris freertos nvs_flash
    PRIV_REQUIRES esp_timer esp-gsp esp_display_present esp_lcd esp_http_client
        esp_http_server esp_wifi esp_netif esp_event esp-tls mbedtls cjson
        esp_codec_dev mosaico_module_camera esp_new_jpeg)
target_add_binary_data(${COMPONENT_LIB} "setup.html" TEXT)
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/gsp_bundle.cmake")
mosaico_gsp_add_ui_bundle(${COMPONENT_LIB} "../ui/main.json")
gsp_enable_freetype()
target_add_binary_data(${COMPONENT_LIB} "../ui/fonts/LearningSans.ttf" BINARY)
