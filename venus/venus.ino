#include "src/common.h"
#include "common.h"
#include "stepper.h"

#ifdef NO_LIMIT_SWITCH
#warning IGNORING LIMIT SWITCH - ASSUMING 0 IS CORRECT
#endif

Mux_Read mux;
Wifi wifi;

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

  while(!Serial) {}

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
 
  // XXX High switches are currently ignored XXX

  steppers[0].init(0, STEP_EN_1, STEP_PULSE_1, STEP_DIR_1, 
                   LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1, DELAY_MIN, DELAY_MAX);
  steppers[1].init(1, STEP_EN_2, STEP_PULSE_2, STEP_DIR_2, 
                   LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2, DELAY_MIN, DELAY_MAX);
  steppers[2].init(2, STEP_EN_3, STEP_PULSE_3, STEP_DIR_3, 
                   LIMIT_SWITCH_LOW_3, LIMIT_SWITCH_HIGH_3, DELAY_MIN, DELAY_MAX);
  steppers[3].init(3, STEP_EN_4, STEP_PULSE_4, STEP_DIR_4, 
                   LIMIT_SWITCH_LOW_4, LIMIT_SWITCH_HIGH_4, DELAY_MIN, DELAY_MAX);

  steppers[0].pos_end = 6000;
  steppers[0].set_backwards();
  steppers[1].set_backwards();
  steppers[2].set_backwards();

  #ifdef USE_PWM_DRIVER

  steppers[4].init(4, STEP_EN_5, STEP_PULSE_5, STEP_DIR_5, 
                   -1, -1, 80, DELAY_MAX);
  steppers[5].init(5, STEP_EN_6, STEP_PULSE_6, STEP_DIR_6, 
                   -1, -1, 80, DELAY_MAX);
  steppers[6].init(6, STEP_EN_7, STEP_PULSE_7, STEP_DIR_7, 
                   -1, -1, 80, DELAY_MAX);
  steppers[7].init(7, STEP_EN_8, STEP_PULSE_8, STEP_DIR_8, 
                   -1, -1, 80, DELAY_MAX);
  steppers[8].init(8, STEP_EN_9, STEP_PULSE_9, STEP_DIR_9, 
                   -1, -1, 80, DELAY_MAX);
  #endif

  Serial.println("Starting...");

  wifi.init();
  init_mode();
}

void init_mode() {
  int i1 = mux.read_switch(INPUT_SWITCH_0);
  int i2 = mux.read_switch(INPUT_SWITCH_1);
  int i3 = mux.read_switch(INPUT_SWITCH_2);

  STEP_STATE mode = DEFAULT_MODE;

  Serial.print("Setting mode to: ");

  switch(i1 + i2 << 1 + i3 << 2) {
    case 1:
      mode = STEP_CLOSE;
      //mode_next = STATE_OPEN;
      Serial.println("close");
      break;
    case 2:
      mode = STEP_OPEN;
      //mode_next = STATE_CLOSE;
      Serial.println("open");
      break;
    case 3:
      mode = STEP_SWEEP;
      Serial.println("sweep");
      break;
    case 4:
      mode = STEP_RANDOM_WALK;
      Serial.println("random walk");
    case 0:
    default:
      Serial.printf("default (%d)\n", mode);
      break;
  };

  for(int i=0; i<NUM_STEPPERS; i++) {
    steppers[i].state_next = mode;
    steppers[i].choose_next();
    steppers[i].state_next = STEP_WIGGLE_START;
  }
}

void benchmark() {
  static uint32_t iterations = 0;
  static float avg = 0;
  static uint32_t last = 0;
  static uint32_t last_output = millis();

  uint32_t now = millis();
  uint32_t diff = now - last;
  last = now;

  if(!iterations)
    avg = diff;
  else
    // Poor-mans moving average
    avg = (avg * 4 + diff)/5;

  // NOTE: This will skew the measurement for the next iteration
  // Need extra work to keep accurate, but that's a pretty low priority
  if(now - last_output < 1000)
    return;

  Serial.printf("Benchmark: %f ms\n", avg);
  last_output = now;
}

void log_inputs() {
  static uint32_t last = 0;
  uint32_t now = millis();

  if(now - last < 500)
    return;

  last = now;

  Serial.printf("Mux:");

  for(int i=0; i<16; i++)
    Serial.printf("\t%d", mux.read_raw(i));

  Serial.println();
}

void loop() {
  blink();
  mux.next();
  benchmark();
  //log_inputs();

  // Check if sensors triggered
  uint32_t sens = mux.read_raw(SENS_IN_4);

  if(sens > SENS_THOLD) {
    //Serial.printf("Triggered: %ld\n", sens);
      for(int i=0; i<1; i++) {
        steppers[i].trigger_close();
      }
  }

  for(int i=0; i<1; i++)
    steppers[i].run();
}
