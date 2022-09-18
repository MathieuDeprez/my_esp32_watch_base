# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

add_compile_definitions(SPI_HOST_MAX=3)
add_compile_definitions(LV_HOR_RES_MAX=240)
add_compile_definitions(LV_VER_RES_MAX=240)

# x_out = x_in*TOUCH_CAL_X_MUL+TOUCH_CAL_X_OFF
add_compile_definitions(TOUCH_CAL_X_OFF=25)
add_compile_definitions(TOUCH_CAL_X_MUL=0.75)
# y_out = y_in*TOUCH_CAL_Y_MUL+TOUCH_CAL_Y_OFF
add_compile_definitions(TOUCH_CAL_Y_OFF=0)
add_compile_definitions(TOUCH_CAL_Y_MUL=1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(watch_display_01)
