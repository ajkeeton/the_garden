#pragma once
#include <Adafruit_PWMServoDriver.h>
#include <stdint.h>

#if USE_PWM_DRIVER
#define ENABLE_1 15
#define ENABLE_2 14
#define ENABLE_3 13

#define STEP_1 15
#define STEP_2 13
#define STEP_3 11

#define DIR_1 14
#define DIR_2 12
#define DIR_3 10
#else
#define ENABLE_1 15
#define STEP_1 14
#define DIR_1 13

#define ENABLE_2 12
#define STEP_2 11
#define DIR_2 10
 
#define ENABLE_3 9
#define STEP_3 8
#define DIR_3 7

#define ENABLE_4 6
#define STEP_4 5
#define DIR_4 4

#endif

#define LIMIT_SWITCH_LOW_1 0
#define LIMIT_SWITCH_HIGH_1 1

#define LIMIT_SWITCH_LOW_2 2
#define LIMIT_SWITCH_HIGH_2 3

#define LIMIT_SWITCH_LOW_3 4
#define LIMIT_SWITCH_HIGH_3 5

#define LIMIT_SWITCH_LOW_4 6
#define LIMIT_SWITCH_HIGH_4 7

#define STEP_FREQ 1000

#define MUX_EN 22
#define MUX4 20
#define MUX3 19
#define MUX2 18
#define MUX1 17

#define MUX_IN1 28

#define DIP_0 0
#define DIP_1 1
