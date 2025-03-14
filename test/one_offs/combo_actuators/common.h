#pragma once
#include <Adafruit_PWMServoDriver.h>
#include <stdint.h>

#define ENABLE_1 15
#define ENABLE_2 14

#define STEP_1 15
#define DIR_1 14

#define STEP_2 13
#define DIR_2 12

#define LIMIT_SWITCH_LOW_1 14
#define LIMIT_SWITCH_HIGH_1 15

#define LIMIT_SWITCH_LOW_2 13
#define LIMIT_SWITCH_HIGH_2 12

#define MUX_EN 22
#define MUX4 20
#define MUX3 19
#define MUX2 18
#define MUX1 17

#define MUX_IN1 28

#define DIP_0 0
#define DIP_1 1

#define SERVOMIN  100 
#define SERVOMAX  400 // 550

#define STEP_FREQ 1000
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

#define SERVO_UPDATE_DELTA 1000

uint16_t mux_read_raw(int pin);
bool mux_read_switch(int pin);
void servos_init();