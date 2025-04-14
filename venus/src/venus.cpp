#include "common/common.h"
#include "common/wifi.h"
#include "stepper.h"
#include "leds.h"

#ifdef NO_LIMIT_SWITCH
#warning IGNORING LIMIT SWITCH - ASSUMING 0 IS CORRECT
#endif

Mux_Read mux;
Wifi wifi;
leds_t leds;

#if USE_PWM_DRIVER
Adafruit_PWMServoDriver step_dir = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver step_en = Adafruit_PWMServoDriver(0x41);
#define NUM_STEPPERS 9
#else
#define NUM_STEPPERS 3
#endif

Stepper steppers[NUM_STEPPERS];

void init_steppers() {
  steppers[0].init(0, STEP_EN_1, STEP_PULSE_1, STEP_DIR_1, 
                   LIMIT_SWITCH_LOW_1, DELAY_MIN, DELAY_MAX);
  steppers[1].init(1, STEP_EN_2, STEP_PULSE_2, STEP_DIR_2, 
                   LIMIT_SWITCH_LOW_2, DELAY_MIN, DELAY_MAX);
  steppers[2].init(2, STEP_EN_3, STEP_PULSE_3, STEP_DIR_3, 
                   LIMIT_SWITCH_LOW_3, DELAY_MIN, DELAY_MAX);
 
  step_settings_t ss;

  ss.pause_ms = 10;
  ss.accel = 0.00015;
  ss.min_delay = 80;
  steppers[0].settings_on_close = ss;
  
  ss.pause_ms = 10;
  steppers[1].settings_on_close = ss;

  ss.accel = 0.00015;
  ss.pause_ms = 400;
  ss.min_delay = 150;
  steppers[2].settings_on_close = ss;

  ss.accel = 0.000002;
  ss.pause_ms = 50;
  ss.min_delay = 750; // scale down based on excitement
  ss.max_delay = 10000;
  ss.min_pos = 0;
  ss.max_pos = 2000; // scale up based on excitement
  steppers[0].settings_on_wiggle = ss;
  steppers[1].settings_on_wiggle = ss;
  steppers[2].settings_on_wiggle = ss;

  steppers[0].set_backwards();
  steppers[1].set_backwards();

  #ifdef SHORT_RAILS
  steppers[2].set_backwards();
  #endif

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
}

void init_mode() {
  int i1 = mux.read_switch(INPUT_SWITCH_0);
  int i2 = mux.read_switch(INPUT_SWITCH_1);
  int i3 = 0; //int i3 = mux.read_switch(INPUT_SWITCH_2);

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
    steppers[i].choose_next(); // necessary to initialize targets, speeds, etc
    steppers[i].state_next = mode; //  DEFAULT_MODE_NEXT; // STEP_SWEEP; // STEP_WIGGLE_START;
  }
}
void wait_serial() {
  uint32_t now = millis();

  // Wait up to 5 seconds for serial
  while(!Serial && millis() - now < 5000) {
    delay(100);
  }  
}

void setup1() {
  Serial.begin(9600);
  wait_serial();

  pinMode(STEP_PULSE_1, OUTPUT);
  pinMode(STEP_PULSE_2, OUTPUT);
  pinMode(STEP_PULSE_3, OUTPUT);

  pinMode(STEP_DIR_1, OUTPUT);
  pinMode(STEP_DIR_2, OUTPUT);
  pinMode(STEP_DIR_3, OUTPUT);

  pinMode(STEP_EN_1, OUTPUT);
  pinMode(STEP_EN_2, OUTPUT);
  pinMode(STEP_EN_3, OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("Starting...");

  mux.init();
  init_steppers();
  init_mode();
}

void setup() {
  wait_serial();
  leds.init();
  wifi.init();
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
  if(now - last_output < 2000)
    return;

  Serial.printf("Benchmark: %f ms\n", avg);
  last_output = now;
}

void log_inputs() {
  static uint32_t last = 0;
  uint32_t now = millis();

  if(now - last < 750)
    return;

  last = now;

  Serial.printf("Mux:");

  for(int i=0; i<16; i++)
    Serial.printf("\t%d", mux.read_raw(i));

  Serial.println();
}

void loop1() {
  blink();
  // benchmark();
  mux.next();
  #ifdef DEBUG_STEP
  log_inputs();
  #endif

  // Check if sensors triggered
  uint32_t sens = mux.read_raw(SENS_IN_1);

  static uint32_t last = 0;
  uint32_t now = millis();

  if(sens > SENS_THOLD) {
      if(now > last + 500) {
        last = now;
        Serial.printf("Triggered: %ld\n", sens);
      }

      for(int i=0; i<NUM_STEPPERS; i++) {
        steppers[i].trigger_close();
      } 
      //delay(1000);
  }

  for(int i=0; i<NUM_STEPPERS; i++)
    steppers[i].run();
}

void loop() {
  leds.step();
  wifi.run();
}
