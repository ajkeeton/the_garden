#pragma once

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <stdint.h>

#define NO_LIMIT_SWITCH

#define SENS_THOLD 400
#define DEFAULT_MAX_STEPS 6000
#define ACCEL_CONST_DIV 5000
#define DELAY_MIN 75 // in MICRO seconds
#define DELAY_MAX 10000 // in MICRO seconds

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

/////////////////////////////////////////
// Following values are for the mux pins
//
// XXX High switches are currently ignored XXX
#define LIMIT_SWITCH_LOW_1 15
#define LIMIT_SWITCH_HIGH_1 14

#define LIMIT_SWITCH_LOW_2 13
#define LIMIT_SWITCH_HIGH_2 12

#define LIMIT_SWITCH_LOW_3 11
#define LIMIT_SWITCH_HIGH_3 10

#define LIMIT_SWITCH_LOW_4 9
#define LIMIT_SWITCH_HIGH_4 8

// Dip switches
#define INPUT_SWITCH_0 0 
#define INPUT_SWITCH_1 1
#define INPUT_SWITCH_2 2

// Sensors
#define SENS_IN_1 3
#define SENS_IN_2 4
#define SENS_IN_3 5
#define SENS_IN_4 9

/////////////////////////////////////////
