#include <Wire.h>
#include "common.h"

#define DELAY_MIN 70 // in MICRO seconds
#define DELAY_MAX 550 // in MICRO seconds
#define INVERT 1
#define SWEEP 1

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
    tdelay = dmin;// * 1000;
    delay_min = dmin;// * 1000;
    delay_max = dmax; // * 1000;

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
    // set_onoff(false); For the stress test, only turn off motor when we're fully extended (pos_end)

    cur_num_steps = 0;
    //bool on_limit = false;

    tpause = 1000000;
    tdelay = delay_min; // random(delay_min, delay_max);

    if(position == pos_end) {
      set_target(0);
      //set_onoff(false);
      Serial.printf("%d: At end\n", idx);
      //delay(1000);
    }
    else {
      set_target(pos_end);
      Serial.printf("%d: At start, cooling off 1 sec\n", idx);
      //set_onoff(false);
      //delay(1000);
    }

    tlast = micros();
  }

  void set_forward(bool f) {
    if(forward != f) {
      tpause = max(30000, tpause); // min 30 ms before reversing
      cur_num_steps = 0;
    }

    forward = f;
    // Serial.printf("direction: %d\n", forward);
    if(forward) {
      digitalWrite(pin_dir, HIGH);
    }
    else {
      digitalWrite(pin_dir, LOW);
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

State state[3];

#define NUM_STEPPERS (sizeof(state)/sizeof(state[0]))

void State::run() {
  uint32_t now = micros();

  if(now < tlast + tdelay + tpause) 
    return;

  tpause = 0;
  tlast = now;

  set_onoff(true);

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

  #if 0
  // https://www.littlechip.co.nz/blog/a-simple-stepper-motor-control-algorithm
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
  #endif

  //Serial.printf("raw: %d\n", tdelay);

  if(tdelay < delay_min)
    tdelay = delay_min;
  if(tdelay > delay_max)
    tdelay = delay_max;

  //Serial.printf("position: %ld vs %ld\n", position, pos_tgt);

  if(position != pos_tgt)
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
 
  state[0].init(0, ENABLE_1, STEP_1, DIR_1, LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1, 80, DELAY_MAX*4);
  state[0].pos_end = 9000;
  state[0].set_target(9000);

  state[1].init(1, ENABLE_2, STEP_2, DIR_2, LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2, 80, DELAY_MAX*4);
  state[1].pos_end = 7000;
  state[1].set_target(7000);

  state[2].init(2, ENABLE_3, STEP_3, DIR_3, LIMIT_SWITCH_LOW_3, LIMIT_SWITCH_HIGH_3, 80, DELAY_MAX*4);
  state[2].pos_end = 7000;
  state[2].set_target(7000);

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
}
