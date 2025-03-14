#pragma once

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <stdint.h>

#define STEP_DEF_END 9000

#define DELAY_MIN 70 // in MICRO seconds
#define DELAY_MAX 550 // in MICRO seconds

// Using a pair of PWM drivers to expand the number of usable outputs
#if USING_PWM_DRIVER

#define STEP_EN_1 1
#define STEP_EN_2 2
#define STEP_EN_3 3
#define STEP_EN_4 4
#define STEP_EN_5 5
#define STEP_EN_6 6
#define STEP_EN_7 7
#define STEP_EN_8 8
#define STEP_EN_9 9

#define STEP_DIR_1 1
#define STEP_DIR_2 2
#define STEP_DIR_3 3
#define STEP_DIR_4 4
#define STEP_DIR_5 5
#define STEP_DIR_6 6
#define STEP_DIR_7 7
#define STEP_DIR_8 8
#define STEP_DIR_9 9

#else

#define STEP_EN_1 15
#define STEP_PULSE_1 14
#define STEP_DIR_1 13

#define STEP_EN_2 12
#define STEP_PULSE_2 11
#define STEP_DIR_2 10
 
#define STEP_EN_3 9
#define STEP_PULSE_3 8
#define STEP_DIR_3 7

#define STEP_EN_4 6
#define STEP_PULSE_4 5
#define STEP_DIR_4 4

#endif

// These inputs are from the mux pins
#define LIMIT_SWITCH_LOW_1 0
#define LIMIT_SWITCH_HIGH_1 1

#define LIMIT_SWITCH_LOW_2 2
#define LIMIT_SWITCH_HIGH_2 3

#define LIMIT_SWITCH_LOW_3 4
#define LIMIT_SWITCH_HIGH_3 5

#define LIMIT_SWITCH_LOW_4 6
#define LIMIT_SWITCH_HIGH_4 7

#define SENS_IN_1 15
#define SENS_IN_2 14
#define SENS_IN_3 13

#define DIP_0 0
#define DIP_1 1

