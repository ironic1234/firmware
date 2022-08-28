/**
 * @file car.h
 * @author Luke Oxley (lcoxley@purdue.edu)
 * @brief  Master Car Control and Safety Checking
 * @version 0.1
 * @date 2022-03-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _CAR_H_
#define _CAR_H_

#include <stdbool.h>
#include "common/phal_L4/gpio/gpio.h"
#include "common/psched/psched.h"
#include "main.h"
#include "can_parse.h"
#include "cooling.h"

#define BUZZER_DURATION_MS 2500 // EV.10.5: 1-3s

#define ERROR_FALL_MS (5000)

/* LV I_SENSE CALC */
#define LV_GAIN         25
#define LV_R_SENSE_mOHM 4 
#define LV_MAX_ADC_RAW  4096
#define LV_ADC_V_IN_V   5

/* BRAKE LIGHT CONFIG */
#define BRAKE_LIGHT_ON_THRESHOLD 500
#define BRAKE_LIGHT_OFF_THRESHOLD 300
#define BRAKE_BLINK_CT 3
#define BRAKE_BLINK_PERIOD 250

/* STEERING ANGLE CALCULATION CONFIG */
#define STEERING_S1 (0.00007)
#define STEERING_S2 (-0.0038)
#define STEERING_S3 (0.6535)
#define STEERING_S4 (0.1061)
#define STEERING_INCH2MM (25.4)
#define STEERING_SLOPE (0.00962)
#define STEERING_DEG2RAD (0.01745329)

/* CAR PARAMETER */
#define STEERING_W 1.269
#define STEERING_L 1.574

/* TUNING PARAMETER */
#define STEERING_K 1.3      // increase k makes the car more oversteer
#define STEERING_P 10.0
#define MOTOR_TORQUE_LIMIT 25.0 // N * m (not used)

typedef struct __attribute__((packed))
{
    // Do not modify this struct unless
    // you modify the ADC DMA config
    // in main.h to match
    uint16_t dt_therm_1;
    uint16_t dt_therm_2;
    uint16_t bat_therm_out;
    uint16_t bat_therm_in;
    uint16_t i_sense_c1;
    uint16_t lv_i_sense;
} ADCReadings_t;

volatile extern ADCReadings_t adc_readings;

typedef struct
{
    int16_t torque_left;
    int16_t torque_right;
} torqueRequest_t;

typedef struct
{
    car_state_t state;
    bool brake_light;
    uint8_t brake_blink_ct;
    uint32_t brake_start_time;
    uint16_t max_cell_temp;
    uint16_t lv_current_mA;
} Car_t;

volatile extern Car_t car;

bool carInit();
void carHeartbeat();
void carPeriodic();

/**
 * @brief Converts raw lv adc value
 * to the current in mA
 */
void calcLVCurrent();

void eDiff(int16_t t_req, torqueRequest_t* torque_r);

#endif