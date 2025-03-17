#include "src/common.h"
#include "common.h"
#include "stepper.h"

Mux_Read mux;

#if USE_PWM_DRIVER
Adafruit_PWMServoDriver step_dir = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver step_en = Adafruit_PWMServoDriver(0x41);
#define NUM_STEPPERS 9
#else
#define NUM_STEPPERS 4
#endif

Stepper steppers[NUM_STEPPERS];

void setup() {
  Serial.begin(9600);

  pinMode(STEP_PULSE_1, OUTPUT);
  pinMode(STEP_PULSE_2, OUTPUT);
  pinMode(STEP_PULSE_3, OUTPUT);
  pinMode(STEP_PULSE_4, OUTPUT);

  pinMode(STEP_DIR_1, OUTPUT);
  pinMode(STEP_DIR_2, OUTPUT);
  pinMode(STEP_DIR_3, OUTPUT);
  pinMode(STEP_DIR_4, OUTPUT);

  pinMode(STEP_EN_1, OUTPUT);
  pinMode(STEP_EN_2, OUTPUT);
  pinMode(STEP_EN_3, OUTPUT);
  pinMode(STEP_EN_4, OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Mux
  pinMode(MUX1, OUTPUT);
  pinMode(MUX2, OUTPUT);
  pinMode(MUX3, OUTPUT);
  pinMode(MUX4, OUTPUT);
  pinMode(MUX_EN, OUTPUT);
  pinMode(MUX_IN1, INPUT);
 
  steppers[0].init(0, STEP_EN_1, STEP_PULSE_1, STEP_DIR_1, 
                   LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1, 80, DELAY_MAX*4);
  steppers[1].init(1, STEP_EN_2, STEP_PULSE_2, STEP_DIR_2, 
                   LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2, 80, DELAY_MAX*4);
  steppers[2].init(2, STEP_EN_3, STEP_PULSE_3, STEP_DIR_3, 
                   LIMIT_SWITCH_LOW_3, LIMIT_SWITCH_HIGH_3, 80, DELAY_MAX*4);
  steppers[3].init(3, STEP_EN_4, STEP_PULSE_4, STEP_DIR_4, 
                   LIMIT_SWITCH_LOW_4, LIMIT_SWITCH_HIGH_4, 80, DELAY_MAX*4);

  // Trial and err'd. Perhaps overwrite remotely at runtime?
  steppers[0].pos_end = 9000;
  steppers[0].set_target(9000);

  steppers[1].pos_end = 7000;
  steppers[1].set_target(7000);

  steppers[2].pos_end = 7000;
  steppers[2].set_target(7000);

  steppers[3].pos_end = 7000;
  steppers[3].set_target(7000);

  #ifdef USE_PWM_DRIVER

  steppers[4].init(4, STEP_EN_5, STEP_PULSE_5, STEP_DIR_5, 
                   -1, -1, 80, DELAY_MAX*4);
  steppers[5].init(5, STEP_EN_6, STEP_PULSE_6, STEP_DIR_6, 
                   -1, -1, 80, DELAY_MAX*4);
  steppers[6].init(6, STEP_EN_7, STEP_PULSE_7, STEP_DIR_7, 
                   -1, -1, 80, DELAY_MAX*4);
  steppers[7].init(7, STEP_EN_8, STEP_PULSE_8, STEP_DIR_8, 
                   -1, -1, 80, DELAY_MAX*4);
  steppers[8].init(8, STEP_EN_9, STEP_PULSE_9, STEP_DIR_9, 
                   -1, -1, 80, DELAY_MAX*4);
  #endif

  Serial.println("Starting...");

  finger_stretch();
}

void finger_stretch() {
  // Make each finger extend and retract independently 
  for(int i=0; i<NUM_STEPPERS; i++)
    steppers[i].state = steppers[i].state_next = STEP_SWEEP; // STEP_FIND_ENDPOINT_1;
}

void loop() {
  blink();

  static uint32_t iterations = 0;
  static float avg = 0;
  uint32_t t = millis();

  for(int i=0; i<NUM_STEPPERS; i++)
    steppers[i].run();
  // steppers[0].run();

  //uint32_t diff = millis() - t;
  //if(!avg)
  //  avg = diff;n
  //else
  //avg = (avg * 4 + diff)/5;
}

