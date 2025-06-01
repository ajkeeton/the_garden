#pragma once

#include <Wire.h>
#include <stdint.h>

#define SENS_THOLD 300

#define DEFAULT_MAX_STEPS_SHORT 6000
#define DEFAULT_MAX_STEPS 8500

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

#define STEP_EN_1 1
#define STEP_PULSE_1 2
#define STEP_DIR_1 3

#define STEP_EN_2 4
#define STEP_PULSE_2 5
#define STEP_DIR_2 6
 
#define STEP_EN_3 7
#define STEP_PULSE_3 8
#define STEP_DIR_3 9

#endif

/////////////////////////////////////////
// Following values are for the *mux* pins
#define LIMIT_SWITCH_LOW_1 0
#define LIMIT_SWITCH_LOW_2 1
#define LIMIT_SWITCH_LOW_3 2

// Dip switches
#define INPUT_SWITCH_0 A1
#define INPUT_SWITCH_1 A2

// Sensors
#define SENS_IN_1 4


/////////////////////////////////////////
