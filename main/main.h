#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nmea_parser.h"

// my classes
#include "WatchTft.h"
#include "WatchPower.h"
#include "WatchBma.h"
#include "WatchNvs.h"
#include "WatchGps.h"

// my cpp
#include "C_AXP202X_Library/axp20x.h"

#include "BMA423-Sensor-API/bma423.h"
#include "BMA423-Sensor-API/examples/generic/common/bma4_common.h"

extern SemaphoreHandle_t i2c_0_semaphore;
extern uint32_t timer_turn_off_screen;

#endif