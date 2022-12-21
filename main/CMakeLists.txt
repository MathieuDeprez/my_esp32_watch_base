file(GLOB_RECURSE SOURCES "*.cpp")
set(LVGL_INCLUDE_DIRS . WatchTft)
list(APPEND LVGL_INCLUDE_DIRS WatchPower)
list(APPEND LVGL_INCLUDE_DIRS WatchBma)
list(APPEND LVGL_INCLUDE_DIRS WatchNvs)
list(APPEND LVGL_INCLUDE_DIRS WatchGps)
list(APPEND LVGL_INCLUDE_DIRS WatchSd)
list(APPEND LVGL_INCLUDE_DIRS WatchWiFi)

idf_component_register(SRCS "main.cpp" ${SOURCES} "WatchTft/Power_Screen.cpp" "WatchTft/Settings_Screen.cpp" "WatchTft/Gps_Screen.cpp" "WatchTft/download.cpp"
                    INCLUDE_DIRS ${LVGL_INCLUDE_DIRS}
                    REQUIRES lvgl lvgl_esp32_drivers C_AXP202X_Library BMA423-Sensor-API nvs_flash fatfs esp_http_client)


 target_compile_options(${COMPONENT_LIB} PRIVATE  -Wno-missing-field-initializers)
