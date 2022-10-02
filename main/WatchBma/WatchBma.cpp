#include "WatchBma.h"
struct bma4_dev WatchBma::bma;
uint32_t WatchBma::steps_count_save = 0;

void WatchBma::init()
{
    xSemaphoreTake(i2c_0_semaphore, portMAX_DELAY);

    uint16_t rslt = BMA4_OK;

    struct bma4_accel_config accel_conf;
    uint8_t dev_addr = BMA4_I2C_ADDR_PRIMARY;

    bma.chip_id = BMA423_CHIP_ID;
    bma.intf = BMA4_I2C_INTF;
    bma.bus_read = user_i2c_read;
    bma.bus_write = user_i2c_write;
    bma.variant = BMA42X_VARIANT;
    bma.intf_ptr = &dev_addr;
    bma.delay_us = user_delay;
    bma.read_write_len = 8;
    bma.resolution = 12;
    bma.feature_len = 64;
    bma.dummy_byte = 0;

    bma4_soft_reset(&bma);
    vTaskDelay(20);

    /* Sensor initialization */
    rslt = bma423_init(&bma);
    bma4_error_codes_print_result("bma423_init", rslt);
    if (rslt != 0)
    {
        printf("error bma423_init: %d\n", rslt);
        vTaskDelay(portMAX_DELAY);
    }

    /* Upload the configuration file to enable the features of the sensor. */
    rslt = bma423_write_config_file(&bma);
    bma4_error_codes_print_result("bma423_write_config", rslt);
    if (rslt != 0)
    {
        printf("error write config: %d\n", rslt);
        vTaskDelay(portMAX_DELAY);
    }

    struct bma4_int_pin_config config;
    config.edge_ctrl = BMA4_LEVEL_TRIGGER;
    config.lvl = BMA4_ACTIVE_HIGH;
    config.od = BMA4_PUSH_PULL;
    config.output_en = BMA4_OUTPUT_ENABLE;
    config.input_en = BMA4_INPUT_DISABLE;
    rslt = bma4_set_int_pin_config(&config, BMA4_INTR1_MAP, &bma);
    if (rslt != 0)
    {
        printf("error bma4_set_int_pin_config: %d\n", rslt);
        vTaskDelay(portMAX_DELAY);
    }

    if (bma4_set_accel_enable(BMA4_ENABLE, &bma))
    {
        return;
    }
    bma4_accel_config cfg;
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    cfg.range = BMA4_ACCEL_RANGE_2G;
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
    cfg.perf_mode = BMA4_CONTINUOUS_MODE;

    if (bma4_set_accel_config(&cfg, &bma))
    {
        printf("[bma4] set accel config fail\n");
        return;
    }
    rslt |= bma423_reset_step_counter(&bma);
    rslt |= bma423_step_detector_enable(BMA4_ENABLE, &bma);
    rslt |= bma423_feature_enable(BMA423_STEP_CNTR, BMA4_ENABLE, &bma);
    rslt |= bma423_feature_enable(0x20, BMA4_ENABLE, &bma);
    rslt |= bma423_feature_enable(0x10, BMA4_ENABLE, &bma);
    rslt |= bma423_step_counter_set_watermark(100, &bma);

    // rslt |= bma423_map_interrupt(BMA4_INTR1_MAP, BMA423_STEP_CNTR_INT | BMA423_WAKEUP_INT, BMA4_ENABLE, &_dev);
    rslt |= bma423_map_interrupt(BMA4_INTR1_MAP, BMA423_STEP_CNTR_INT, BMA4_ENABLE, &bma);
    rslt |= bma423_map_interrupt(BMA4_INTR1_MAP, 0x08, BMA4_ENABLE, &bma);

    uint8_t feature_config[BMA423_ANYMOTION_EN_LEN + 2] = {0};
    /* Anymotion axis enable bit pos. is 3 byte ahead of the
      anymotion base address(0x00) */
    uint8_t index = 3;

    if (bma.chip_id == BMA423_CHIP_ID)
    {
        rslt = bma4_read_regs(BMA4_FEATURE_CONFIG_ADDR, feature_config,
                              BMA423_ANYMOTION_EN_LEN + 2, &bma);
        if (rslt == BMA4_OK)
        {
            feature_config[index] = BMA4_SET_BITSLICE(feature_config[index],
                                                      BMA423_ANY_NO_MOTION_AXIS_EN, 0);
            rslt |= bma4_write_regs(BMA4_FEATURE_CONFIG_ADDR, feature_config,
                                    BMA423_ANYMOTION_EN_LEN + 2, &bma);
        }
    }
    else
    {
        rslt = BMA4_E_INVALID_SENSOR;
    }

    /* Enable the accelerometer */
    rslt = bma4_set_accel_enable(1, &bma);
    bma4_error_codes_print_result("bma4_set_accel_enable status", rslt);

    /* Accelerometer Configuration Setting */
    /* Output data Rate */
    accel_conf.odr = BMA4_OUTPUT_DATA_RATE_100HZ;

    /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G) */
    accel_conf.range = BMA4_ACCEL_RANGE_2G;

    /* Bandwidth configure number of sensor samples required to average
     * if value = 2, then 4 samples are averaged
     * averaged samples = 2^(val(accel bandwidth))
     * Note1 : More info refer datasheets
     * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but since the
     * performance power mode phase is increased, the power consumption will also rise.
     */
    accel_conf.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

    /* Enable the filter performance mode where averaging of samples
     * will be done based on above set bandwidth and ODR.
     * There are two modes
     *  0 -> Averaging samples (Default)
     *  1 -> No averaging
     * For more info on No Averaging mode refer datasheets.
     */
    accel_conf.perf_mode = BMA4_CIC_AVG_MODE;

    /* Set the accel configurations */
    rslt = bma4_set_accel_config(&accel_conf, &bma);
    bma4_error_codes_print_result("bma4_set_accel_config status", rslt);

    /* Enable step counter */
    rslt = bma423_feature_enable(BMA423_STEP_CNTR, 1, &bma);
    bma4_error_codes_print_result("bma423_feature_enable status", rslt);

    /* Map the interrupt pin with that of step counter interrupts
     * Interrupt will  be generated when step activity is generated.
     */
    rslt = bma423_map_interrupt(BMA4_INTR1_MAP, BMA423_STEP_CNTR_INT, 1, &bma);
    bma4_error_codes_print_result("bma423_map_interrupt status", rslt);

    /* Set water-mark level 1 to get interrupt after 20 steps.
     * Range of step counter interrupt is 0 to 20460(resolution of 20 steps).
     */
    rslt = bma423_step_counter_set_watermark(1, &bma);
    bma4_error_codes_print_result("bma423_step_counter status", rslt);

    rslt = bma423_step_detector_enable(1, &bma);
    bma4_error_codes_print_result("bma423_step_detector_enable status", rslt);

    xSemaphoreGive(i2c_0_semaphore);
}

uint32_t WatchBma::get_steps()
{
    BaseType_t err = xSemaphoreTake(i2c_0_semaphore, 500);
    if (err != pdTRUE)
    {
        return 0;
    }
    uint32_t step_out = 0;
    int8_t rslt = bma423_step_counter_output(&step_out, &bma);
    printf("Step counter: %u\n", step_out);
    bma4_error_codes_print_result("bma423_step_counter_output status", rslt);
    xSemaphoreGive(i2c_0_semaphore);
    return step_out;
}