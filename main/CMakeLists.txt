file(GLOB_RECURSE SOURCES "*.cpp")
set(LVGL_INCLUDE_DIRS . WatchTft)
list(APPEND LVGL_INCLUDE_DIRS WatchPower)

idf_component_register(SRCS "main.cpp" ${SOURCES}
                    INCLUDE_DIRS ${LVGL_INCLUDE_DIRS}
                    REQUIRES lvgl lvgl_esp32_drivers C_AXP202X_Library)


 target_compile_options(${COMPONENT_LIB} PRIVATE  -Wno-missing-field-initializers)
