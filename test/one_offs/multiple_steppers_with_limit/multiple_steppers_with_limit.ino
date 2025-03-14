#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

#define ENABLE_1 15
#define STEP_1 15
#define DIR_1 14

#define ENABLE_2 14
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

#define SENS1 2

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

void setup() {
  Serial.begin(9600);
  //while(!Serial) {}
  Serial.println("16 channel PWM test!");

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
  
  pwm1.begin();
  pwm1.setPWMFreq(750);

  pwm2.begin();
  pwm2.setPWMFreq(750);
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
    pwm2.setPWM(pin, 4096, 0);
  else
    pwm2.setPWM(pin, 0, 4096);
}

void set_dir(uint8_t pin_step, uint8_t pin_dir, bool forward) {
  static bool last = false;

  Serial.printf("direction: %d\n", forward);

  if(last != forward) {
    pwm1.setPWM(pin_step, 0, 4096); // Turn off motor momentarily
    delay(20);
    last = forward;
  }

  if(forward) {
    pwm1.setPWM(pin_dir, 4096, 0); // Direction pin, 100% high
  }
  else {
    pwm1.setPWM(pin_dir, 0, 4096); // Direction pin, 100% low
  }
}

uint16_t muxRead(int pin) {
  digitalWrite(MUX_EN, HIGH);
  digitalWrite(MUX1, pin & 1);
  digitalWrite(MUX2, pin & 2);
  digitalWrite(MUX3, pin & 4);
  digitalWrite(MUX4, pin & 8);
  digitalWrite(MUX_EN, LOW);
  delay(1);
  return analogRead(MUX_IN1);
}

bool check_limit(int pin) {
  //Serial.printf("Limit: %d\n", analogRead(pin));
  //return analogRead(pin) > 2048;
  Serial.printf("Limit %d: %d\n", pin, muxRead(pin));
  return muxRead(pin) > 512;
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

  Serial.printf("Sensor: %d\n", muxRead(SENS1));
  //if(muxRead(SENS1) < 200)
    //enable(false);
  //else
    enable(en, true);

  //pwm1.setPWM(STEP, 2048, 4096); // 50% duty cycle, I think
  pwm1.setPWM(step, 1024, 4096-1024); // 25% duty cycle, I think

  delay(10);
}

void loop() {
  do_stepper(LIMIT_SWITCH_LOW_1, LIMIT_SWITCH_HIGH_1, ENABLE_1, DIR_1, STEP_1);
  do_stepper(LIMIT_SWITCH_LOW_2, LIMIT_SWITCH_HIGH_2, ENABLE_2, DIR_2, STEP_2);
}
