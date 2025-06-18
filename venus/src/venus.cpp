#define WADS_V2A_BOARD

#include "common/common.h"
#include "common/minmax.h"
#include "common/wifi.h"
#include "stepper.h"
#include "leds.h"

#ifdef NO_LIMIT_SWITCH
#warning IGNORING LIMIT SWITCH - ASSUMING 0 IS CORRECT
#endif

mux_t mux;
wifi_t wifi;
leds_t leds;
min_max_range_t minmax(100, 1000); // Default min/max for the sensors

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
  ss.accel = 0.00025;
  ss.min_delay = 80;
  steppers[0].settings_on_close = ss;
  
  ss.pause_ms = 10;
  steppers[1].settings_on_close = ss;

  // Wait to close
  ss.accel = 0.000025;
  ss.pause_ms = 300;
  ss.min_delay = 100;
  steppers[2].settings_on_close = ss;

  // Open slower
  ss.pause_ms = 250;
  ss.min_delay = 150;
  ss.accel = 0.000025;
  steppers[0].settings_on_open = ss;
  steppers[1].settings_on_close = ss;

  // Open faster than the others
  ss.pause_ms = 10;
  ss.min_delay = 50;
  ss.accel = 0.00015;
  steppers[2].settings_on_open = ss;

  ss.accel = 0.000002;
  ss.pause_ms = 50;
  ss.min_delay = 750; // XXX scale down based on excitement
  ss.max_delay = 10000;
  ss.min_pos = 200;
  ss.max_pos = DEFAULT_MAX_STEPS * .3; // XXX scale up based on excitement
  steppers[0].settings_on_wiggle = ss;
  steppers[1].settings_on_wiggle = ss;
  steppers[2].settings_on_wiggle = ss;

  if(analogRead(INPUT_SWITCH_0) < 100) {
    Serial.printf("Short rails selected\n");
    // If dip 0 is set, use "short_rails"

    steppers[0].set_short(true);
    steppers[1].set_short(true);
    steppers[2].set_short(true);
  }
  else if (analogRead(INPUT_SWITCH_1) < 100) {
    // For limb C only!
    Serial.printf("Pod C selected\n");
    steppers[1].set_backwards();
    steppers[2].set_backwards();
  }

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
  STEP_STATE mode = DEFAULT_MODE;
  #if 0
  int i1 = mux.read_switch(INPUT_SWITCH_0);
  int i2 = mux.read_switch(INPUT_SWITCH_1);
  int i3 = 0; //int i3 = mux.read_switch(INPUT_SWITCH_2);


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
  #endif

  for(int i=0; i<NUM_STEPPERS; i++) {
    steppers[i].choose_next(mode); // necessary to initialize targets, speeds, etc
    // steppers[i].state_next = mode; //  DEFAULT_MODE_NEXT; // STEP_SWEEP; // STEP_WIGGLE_START;
  }
}
void wait_serial() {
  uint32_t now = millis();

  // Wait up to 2 seconds for serial
  while(!Serial && millis() - now < 2000) {
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
  mux.num_inputs = 6;
  init_steppers();
  init_mode();
  leds.init();

  minmax.init_avg(mux.read_raw(SENS_IN_1));
}

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);

  //pinMode(15, OUTPUT);
  //digitalWrite(15, HIGH);

  wait_serial();
  wifi.init("venus");

  //while(true) { yield() ; delay(100); }
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

  mux.log_info();
}

void loop1() {
  blink();
  // benchmark();
  mux.next();

  log_inputs();

  // TODO: handle sensor override button 
  uint32_t sens = mux.read_raw(SENS_IN_1);

  minmax.update(sens);
  // Serial.printf("Sensor: %d, Min: %d, Max: %d\n", sens, minmax.avg_min, minmax.avg_max);
  minmax.log_info();

  static uint32_t last = 0;
  uint32_t now = millis();

  if(minmax.triggered_at(sens)) {
      if(now > last + 500) {
        last = now;
        Serial.printf("Triggered: %ld (%.2f std: %.2f)\n", sens, minmax.pseudo_avg, minmax.std_dev);
      }

      for(int i=0; i<NUM_STEPPERS; i++) {
        steppers[i].trigger_close();
      } 
  }

  for(int i=0; i<NUM_STEPPERS; i++)
    steppers[i].run();
  
  // Move cores?
  leds.background_update();
  leds.step();
}

void loop() {
  wifi.next();
  
  recv_msg_t msg;
  while(wifi.recv_pop(msg)) {
      switch(msg.type) {
          case PROTO_PULSE:
              Serial.printf("The gardener told us to pulse: %u %u %u %u\n", 
                  msg.pulse.color, msg.pulse.fade, msg.pulse.spread, msg.pulse.delay);
              break;
          case PROTO_STATE_UPDATE:
              Serial.printf("State update: %u %u\n", 
                      msg.state.pattern_idx, msg.state.score);
              break;
          case PROTO_PIR_TRIGGERED:
              Serial.println("A PIR sensor was triggered");
              break;
          case PROTO_SLEEPY_TIME:
              Serial.println("Received sleepy time message");
              break;
          default:
              Serial.printf("Ignoring message type: %u\n", msg.type);
      }
  }
}
