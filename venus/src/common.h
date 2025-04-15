#pragma once

#include <Wire.h>
#include <stdint.h>

//#define NO_LIMIT_SWITCH
#define SENS_THOLD 300

#define SHORT_RAILS
#ifdef SHORT_RAILS
#define DEFAULT_MAX_STEPS 6000
#else
#define DEFAULT_MAX_STEPS 8500
#endif

#define DELAY_MIN 80 // in MICRO seconds
#define DELAY_MAX 20000 // in MICRO seconds

// Using a pair of PWM drivers to expand the number of usable outputs
#if USING_PWM_DRIVER
#include <Adafruit_PWMServoDriver.h>

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

#endif

/////////////////////////////////////////
// Following values are for the *mux* pins
//
//#define LIMIT_SWITCH_LOW_1 15
//#define LIMIT_SWITCH_LOW_2 14
//#define LIMIT_SWITCH_LOW_3 13

#define LIMIT_SWITCH_LOW_1 12
#define LIMIT_SWITCH_LOW_2 11
#define LIMIT_SWITCH_LOW_3 10

// Dip switches
#define INPUT_SWITCH_0 15 
#define INPUT_SWITCH_1 14

// Sensors
#define SENS_IN_1 13
//#define SENS_IN_2 14
//#define SENS_IN_3 13

/////////////////////////////////////////
