#if 0
#include <Wire.h>
#include "common.h"

Adafruit_PWMServoDriver step_dir = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver step_en = Adafruit_PWMServoDriver(0x41);

const int p = 3, i = 1, d = 1; // using int arithmetic. Values are treated as 1/x

class State {
public:
  int32_t position = 0,
          last_err = 0;
  bool last_dir = true;

  // Set when we hit a limit switch
  int32_t pulse_at_end = 0, 
          pulse_at_begin = 0;

  int32_t target = 0,
          speed = 0,
          last = 0,
          pause = 0;

  int pin_step = 0,
      pin_dir = 0,
      pin_enable = 0,
      pin_switch_low = 0,
      pin_switch_high = 0;

 // bool at_limit = false;

  void init(int en, int step, int dir, int lsl, int lsh) {
    pin_enable = en;
    pin_step = step;
    pin_dir = dir;
    pin_switch_low = lsl;
    pin_switch_high = lsh;
  }

  bool check_limit(int pin) {
    //Serial.printf("Limit %d: %d\n", pin, mux_read_raw(pin));
    return mux_read_switch(pin);
  }

  void check_limit() {
    // When we hit a limit switch, stop moving
    // check and-or update the end points

    if(check_limit(pin_switch_low)) {
      set_onoff(false);
      pulse_at_begin = position;

      Serial.println("Limit switch - low");
      target = position; // pulse_high - random(100);
    }

    if(check_limit(pin_switch_high)) {
      set_onoff(false);
      pulse_at_end = position;

      Serial.println("Limit switch - high");
      target = position; // pulse_low + random(100);
    }
  }

  void set_onoff(bool onoff) {
    if(onoff)
      step_en.setPWM(pin_enable, 4096, 0);
    else {
      step_en.setPWM(pin_enable, 0, 4096);
      last_err = 0;
      target = 0;
      pause = 20;
    }
  }

  void choose_next() {
    if(pulse_at_end)
      target = random(pulse_at_begin, pulse_at_end);
    else
      target = random(pulse_at_begin, position+500); // this will allow us to work our way to the end

    pause = random(20, 1500);

    // RETHINK: implement based on "run direction X for Y ms", when endpoints are discovered
    // Bit bang, per cycle

    set_onoff(true);
    step_dir.setPWM(pin_step, 1024, 4096-1024); 
  }

  void set_forward(int err, bool forward) {
    // Serial.printf("direction: %d\n", forward);

    if(last_dir != forward) {
      step_dir.setPWM(pin_step, 0, 4096); // Turn off motor momentarily
      delay(20);
      last_dir = forward;
    }

    if(forward) {
      step_dir.setPWM(pin_dir, 4096, 0); // Direction pin, 100% high
    }
    else {
      step_dir.setPWM(pin_dir, 0, 4096); // Direction pin, 100% low
    }
  }

  void run();
};

State state[3];

#define NUM_STEPPERS (sizeof(state)/sizeof(state[0]))

void setup() {
  Serial.begin(9600);

  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Mux
  pinMode(MUX1, OUTPUT);
  pinMode(MUX2, OUTPUT);
  pinMode(MUX3, OUTPUT);
  pinMode(MUX4, OUTPUT);
  pinMode(MUX_EN, OUTPUT);
  pinMode(MUX_IN1, INPUT);
  
  step_dir.begin();
  step_dir.setPWMFreq(STEP_FREQ);

  step_en.begin();
  step_en.setPWMFreq(STEP_FREQ);

  state[0].init(ENABLE_1, STEP_1, DIR_1, LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1);
  state[1].init(ENABLE_2, STEP_2, DIR_2, LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2);
  state[2].init(ENABLE_3, STEP_3, DIR_3, LIMIT_SWITCH_LOW_3, LIMIT_SWITCH_HIGH_3);

  Serial.println("Starting...");
}

uint16_t mux_read_raw(int pin) {
  digitalWrite(MUX_EN, HIGH);
  digitalWrite(MUX1, pin & 1);
  digitalWrite(MUX2, pin & 2);
  digitalWrite(MUX3, pin & 4);
  digitalWrite(MUX4, pin & 8);
  digitalWrite(MUX_EN, LOW);
  delay(1);
  return analogRead(MUX_IN1);
}

bool mux_read_switch(int pin) {
  return mux_read_raw(pin) > 512;
}

void State::run() {
  check_limit();

  uint32_t now = millis();
  if(now < last + pause)
    return;
  pause = 0;

  //uint32_t lapsed = last - now;
  float lapsed = now - lapsed;
  // convert ms to sec
  lapsed /= 1000;
  if(lapsed < 1)
    return;

  last = now;

  if(!pin_enable)
    return;

  uint32_t steps = STEP_FREQ / lapsed;

  if(last_err > 0)
    position -= steps;
  else
    position += steps;

  int32_t err = position - target;
  err /= p;

  if(err > last_err) {
    // handle overshoot
  }
  last_err = err;

  if(err > 0) {
    set_forward(false, err);
  }
  else if(err < 0) {
    set_forward(true, err);
  }
  
  //next -= err / p; // + next/i + next/d;

  if(position == target) {
    choose_next();
  }
}

void loop() {
  static uint32_t iterations = 0;
  static float avg = 0;
  uint32_t t = millis();

  for(int i=0; i<NUM_STEPPERS; i++)
    state[i].run();

  uint32_t diff = millis() - t;
  //if(!avg)
  //  avg = diff;
  //else
    avg = (avg * 4 + diff)/5;

  iterations++;
  if(!(iterations % 1000))
    Serial.printf("delta: %f %u\n", avg, diff);
}
#endif