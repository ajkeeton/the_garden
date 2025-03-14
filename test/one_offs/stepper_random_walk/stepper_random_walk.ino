#include <Wire.h>
#include "common.h"

#define DELAY_MIN 60 // in MICRO seconds
#define DELAY_MAX 500 // in MICRO seconds
#define INVERT 1
#define SWEEP 1

#if USE_PWM_DRIVER
Adafruit_PWMServoDriver step_dir = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver step_en = Adafruit_PWMServoDriver(0x41);
#endif

const int p = 3, i = 1, d = 1; // using int arithmetic. Values are treated as 1/x

class Mux_Read {
public:
  int vals[16];
  int idx = 0;
  uint32_t last = micros(), 
           delay = 100; // microseconds to wait before next read

  Mux_Read() {
    for(int i=0; i<16; i++)
      vals[i] = 0;

    address(0);
  }

  void address(int pin) {
    digitalWrite(MUX_EN, HIGH);
    digitalWrite(MUX1, pin & 1);
    digitalWrite(MUX2, pin & 2);
    digitalWrite(MUX3, pin & 4);
    digitalWrite(MUX4, pin & 8);
    digitalWrite(MUX_EN, LOW);
  }

  void next() {
    uint32_t now = micros();
    if(now < last + delay)
      return;
    last = now;

    vals[idx] = analogRead(MUX_IN1);

    idx++;
    if(idx > 15)
      idx = 0;

    address(idx);
  }

  bool read_switch(int pin) {
    next();
    return vals[pin] > 512;
  }
};

Mux_Read mux;

class State {
public:
  uint16_t idx = 0;

  uint32_t tlast = 0,   // time of last step
           tdelay = 0,  // delay before changing step
           tpause = 0,  // time to pause before making a change
           tignore_limit = 0,
           delay_min = DELAY_MIN,
           delay_max = DELAY_MAX; 

  int32_t position = 0, // Tracks position so we can accel/decel strategic points
          pos_end = INT_MAX,  // Set when we hit the endpoint
          pos_tgt = 0,
          cur_num_steps = 0,
          pos_decel = 0;
  
  bool forward = true;
  int state_step = 0;

  bool last_dir = true,
       was_on = false;

  int pin_step = 0,
      pin_dir = 0,
      pin_enable = 0,
      pin_switch_low = 0,
      pin_switch_high = 0;

  void init(int i, int en, int step, int dir, int lsl, int lsh, int dmin = DELAY_MIN, int dmax = DELAY_MAX) {
    idx = i;
    pin_enable = en;
    pin_step = step;
    pin_dir = dir;
    pin_switch_low = lsl;
    pin_switch_high = lsh;
    tdelay = dmin;
    delay_min = dmin;
    delay_max = dmax;

    was_on = true;
    set_onoff(false);

    set_target(-INT_MAX); // set target to low val so we start by discovering 
                          // find the lower limit
  }

  bool check_limit(int pin) {
    //Serial.printf("Limit %d: %d\n", pin, mux_read_raw(pin));
    return mux.read_switch(pin);
  }

  void set_onoff(bool onoff) {
    onoff = true;
    if(onoff) {
      if(!was_on) {
        was_on = true;
        #if USE_PWM_DRIVER
        step_en.setPWM(pin_enable, 4096, 0);
        #else
        digitalWrite(pin_enable, LOW);
        #endif
      }
    }
    else {
      if(was_on) {
        was_on = false;
        #if USE_PWM_DRIVER
        step_en.setPWM(pin_enable, 0, 4096);
        #else
        digitalWrite(pin_enable, HIGH);
        #endif
      }

      tpause = 20000;
    }
  }

  void choose_next() {
    choose_next_rand_walk();
  }

  void choose_next_rand_walk() {
    set_onoff(false);

    cur_num_steps = 0;
    //bool on_limit = false;
    tlast = micros();

    tpause = random(20000, 1500000);
    //tpause = 2000000; //  20000; // 20 ms
    tdelay = random(delay_min+100, delay_max-50);
    //tdelay = 200;

    #if SWEEP
    if(position == pos_end)
      set_target(0);
    else
      set_target(pos_end);
    #else
    if(pos_end) {
      set_target(random(0, pos_end));
    }
    else {
      set_target(random()); // Just move until we find the end
    }
    #endif
  }

  void set_forward(bool f) {
    if(forward != f) {
      tpause = 30000; // 30 ms before reversing
      cur_num_steps = 0;
    }

    forward = f;
    // Serial.printf("direction: %d\n", forward);
    if(forward) {
      #if USE_PWM_DRIVER
      #if INVERT 
      step_dir.setPWM(pin_dir, 0, 4096); // Direction pin, 100% low
      #else
      step_dir.setPWM(pin_dir, 4096, 0); // Direction pin, 100% high
      #endif
      #else
      digitalWrite(pin_dir, HIGH);
      #endif
    }
    else {
      #if USE_PWM_DRIVER
      #if INVERT
      step_dir.setPWM(pin_dir, 4096, 0); // Direction pin, 100% high
      #else
      step_dir.setPWM(pin_dir, 0, 4096); // Direction pin, 100% low
      #endif
      #else
      digitalWrite(pin_dir, LOW);
      #endif
    }
  }

  void set_target(int32_t tgt) {
    pos_tgt = tgt;

    if(pos_tgt > position)
      pos_decel = (pos_tgt - position) * .9;
    else
      pos_decel = (position - pos_tgt) * .9;
  
    if(pos_tgt < position)
      set_forward(false);
    else
      set_forward(true);
    
    cur_num_steps = 0;
    
    Serial.printf("%d: Position: %d, New target: %d End: %d fwd/back: %d\n", idx, position, pos_tgt, pos_end, forward);
  }

  void run();
};

State state[4];

#define NUM_STEPPERS (sizeof(state)/sizeof(state[0]))

void State::run() {
  #if NO_MOVE
  if(check_limit(pin_switch_low)) {
    Serial.printf("%d: Limit low\n", idx);
  }

  if(check_limit(pin_switch_high)) {
    Serial.printf("%d: Limit high, end pos: %u\n", idx, pos_end);
  }

  return;
  #endif

  uint32_t now = micros();

  if(now < tlast + tdelay + tpause) 
    return;

  tpause = 0;
  tlast = now;

  set_onoff(true);

  if(check_limit(pin_switch_low)) {
    position = 0;
    
    if(!forward) {
      set_forward(true);
      Serial.printf("%d: Limit low\n", idx);
      choose_next();
      return;
    }
  }

  if(check_limit(pin_switch_high)) {
    if(position > 0)
      pos_end = position;

    if(forward) {
      set_forward(false);
      Serial.printf("%d: Limit high, end pos: %u\n", idx, pos_end);
      choose_next();
      return;
    }
  }

  if(forward)
    position++;
  else
    position--;

  digitalWrite(pin_step, state_step);
  state_step = !state_step;
  cur_num_steps++;

  //if(!(cur_num_steps % 2500))
  //  Serial.printf("delay: %u, position: %d, fwd/back: %d, num_step: %d, tgt: %d, decel @: %d\n", 
  ///    tdelay, position, forward, cur_num_steps, pos_tgt, pos_decel);

  //if(position > pos_end)
  //  pos_end = position; // Once we hit the end limit switch this will be correct

  // accel
  if(cur_num_steps < pos_decel) {
    //Serial.print("accel - ");
    //tdelay = tdelay * (4 * cur_num_steps - 1) / (4 * cur_num_steps + 1);
    //tdelay = tdelay * (4 * cur_num_steps) / (4 * cur_num_steps + 1);
    tdelay--;
  }
  // decel
  else
    //tdelay = tdelay * (4 * cur_num_steps + 1) / (4 * cur_num_steps - 1);
    //tdelay = tdelay * (4 * cur_num_steps + 1) / (4 * cur_num_steps);
    tdelay++;

  //Serial.printf("raw: %d\n", tdelay);

  if(tdelay < delay_min)
    tdelay = delay_min;
  if(tdelay > delay_max)
    tdelay = delay_max;

  if(cur_num_steps < 5 || abs(position - pos_tgt) > 5)
    return;

  Serial.printf("At target! Pos: %d Num steps: %d\n", position, cur_num_steps);
  choose_next();

  //delay(5000);
}

void setup() {
  Serial.begin(9600);
  pinMode(STEP_1, OUTPUT);
  pinMode(STEP_2, OUTPUT);
  pinMode(STEP_3, OUTPUT);
  pinMode(STEP_4, OUTPUT);

  #if USE_PWM_DRIVER
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();
  #else
  pinMode(DIR_1, OUTPUT);
  pinMode(ENABLE_1, OUTPUT);
  pinMode(DIR_2, OUTPUT);
  pinMode(ENABLE_2, OUTPUT);
  pinMode(DIR_3, OUTPUT);
  pinMode(ENABLE_3, OUTPUT);
  pinMode(DIR_4, OUTPUT);
  pinMode(ENABLE_4, OUTPUT);
  #endif

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Mux
  pinMode(MUX1, OUTPUT);
  pinMode(MUX2, OUTPUT);
  pinMode(MUX3, OUTPUT);
  pinMode(MUX4, OUTPUT);
  pinMode(MUX_EN, OUTPUT);
  pinMode(MUX_IN1, INPUT);
  
  #if USE_PWM_DRIVER
  step_dir.begin();
  step_dir.setPWMFreq(STEP_FREQ);

  step_en.begin();
  step_en.setPWMFreq(STEP_FREQ);
  #endif

  state[0].init(0, ENABLE_1, STEP_1, DIR_1, LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1);
  state[1].init(1, ENABLE_2, STEP_2, DIR_2, LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2, DELAY_MIN, DELAY_MAX*4);
  state[1].pos_end = 3200;
  state[1].set_target(3200);
  state[2].init(2, ENABLE_3, STEP_3, DIR_3, LIMIT_SWITCH_LOW_3, LIMIT_SWITCH_HIGH_3);
  state[3].init(3, ENABLE_4, STEP_4, DIR_4, LIMIT_SWITCH_LOW_4, LIMIT_SWITCH_HIGH_4);

  Serial.println("Starting...");
}

void loop() {
  static uint32_t iterations = 0;
  static float avg = 0;
  uint32_t t = millis();

  for(int i=0; i<NUM_STEPPERS; i++)
    state[i].run();

  uint32_t diff = millis() - t;
  //if(!avg)
  //  avg = diff;n
  //else
  avg = (avg * 4 + diff)/5;

  iterations++;
  if(!(iterations % 30000))
    Serial.printf("delta: %f %u\n", avg, diff);
}
