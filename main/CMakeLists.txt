file(GLOB_RECURSE SOURCES "*.cpp")
set(LVGL_INCLUDE_DIRS . WatchTft)
list(APPEND LVGL_INCLUDE_DIRS WatchPower)
list(APPEND LVGL_INCLUDE_DIRS WatchBma)
list(APPEND LVGL_INCLUDE_DIRS WatchNvs)

idf_component_register(SRCS "main.cpp" ${SOURCES} "WatchTft/Power_Screen.cpp" "WatchTft/Settings_Screen.cpp"
                    INCLUDE_DIRS ${LVGL_INCLUDE_DIRS}
                    REQUIRES lvgl lvgl_esp32_drivers C_AXP202X_Library BMA423-Sensor-API nvs_flash)


 target_compile_options(${COMPONENT_LIB} PRIVATE  -Wno-missing-field-initializers)
