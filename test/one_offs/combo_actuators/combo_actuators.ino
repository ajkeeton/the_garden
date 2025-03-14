#include <Wire.h>
#include "common.h"

Adafruit_PWMServoDriver step_dir = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver step_en = Adafruit_PWMServoDriver(0x41);
Adafruit_PWMServoDriver servos = Adafruit_PWMServoDriver(0x42);

void do_servos();

void setup() {
  Serial.begin(9600);
  //while(!Serial) {}

  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  i2c_scan();

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

  servos_init();

  Serial.println("Starting...");
}

void i2c_scan() {
  Serial.println("Scanning I2C...");

  for(int addr = 1; addr < 127; addr++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(addr);
    int err = Wire.endTransmission();

    if (!err) {
      Serial.print("I2C device found at 0x");
      if (addr<16)
        Serial.print("0");
      Serial.print(addr, HEX);
      Serial.println("  !");
    }
    else if (err==4)
    {
      Serial.print("Unknown error at address 0x");
      if (addr<16)
        Serial.print("0");
      Serial.println(addr, HEX);
    }
  }
}

void enable(uint8_t pin, bool onoff) {
  Serial.printf("onoff: %d\n", onoff);

  if(onoff)
    step_en.setPWM(pin, 4096, 0);
  else
    step_en.setPWM(pin, 0, 4096);
}

void set_dir(uint8_t pin_step, uint8_t pin_dir, bool forward) {
  static bool last = false;

  Serial.printf("direction: %d\n", forward);

  if(last != forward) {
    step_dir.setPWM(pin_step, 0, 4096); // Turn off motor momentarily
    delay(20);
    last = forward;
  }

  if(forward) {
    step_dir.setPWM(pin_dir, 4096, 0); // Direction pin, 100% high
  }
  else {
    step_dir.setPWM(pin_dir, 0, 4096); // Direction pin, 100% low
  }
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

bool check_limit(int pin) {
  Serial.printf("Limit %d: %d\n", pin, mux_read_raw(pin));
  return mux_read_switch(pin);
}

/*
void back_and_forth() {
  bool limit = false;

  if(check_limit(LIMIT_SWITCH_LOW)) {
    Serial.println("Limit switch - low");
    dir(true);
    limit = true;
  }

  if(check_limit(LIMIT_SWITCH_HIGH)) {
    Serial.println("Limit switch - high");
    dir(false);
    limit = true;
  }

  enable(true);
  //pwm1.setPWM(STEP, 2048, 4096); // 50% duty cycle, I think
  pwm1.setPWM(STEP, 1024, 4096-1024); // 25% duty cycle, I think

  delay(10);
}
*/

void do_stepper(uint8_t low, uint8_t high, uint8_t en, uint8_t dir, uint8_t step) {
  bool limit = false;

  if(check_limit(low)) {
    Serial.println("Limit switch - low");
    set_dir(step, dir, true);
    limit = true;
  }

  if(check_limit(high)) {
    Serial.println("Limit switch - high");
    set_dir(step, dir, false);
    limit = true;
  }

  //Serial.printf("IR sens: %d\n", mux_read_raw(SENS1));

  //if(muxRead(SENS1) < 200)
    //enable(false);
  //else
    enable(en, true);

  //pwm1.setPWM(STEP, 2048, 4096); // 50% duty cycle, I think
  step_dir.setPWM(step, 1024, 4096-1024); // 25% duty cycle, I think

  delay(10);
}

void loop() {
  do_stepper(LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1, ENABLE_1, DIR_1, STEP_1);
  do_stepper(LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2, ENABLE_2, DIR_2, STEP_2);
  do_servos();
}
