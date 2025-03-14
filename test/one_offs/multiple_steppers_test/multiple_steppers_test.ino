#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

#define STEP 15
#define DIR 14

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

void setup() {
  Serial.begin(9600);
  while(!Serial) {}

  Serial.println("16 channel PWM test!");

  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  i2c_scan();

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

void enable(bool onoff) {
  Serial.printf("onoff: %d\n", onoff);

  if(onoff)
    pwm2.setPWM(15, 4096, 0);
  else
    pwm2.setPWM(15, 0, 4096);
}

void dir(bool forward) {
  Serial.printf("direction: %d\n", forward);

  if(forward)
    pwm1.setPWM(DIR, 4096, 0);
  else
    pwm1.setPWM(DIR, 0, 4096);
}

void loop() {
  dir(true);
  enable(true);  
  //pwm1.setPWM(STEP, 2048, 4096); // 50% duty cycle, I think
  pwm1.setPWM(STEP, 1024, 4096-1024); // 25% duty cycle, I think

  delay(1000);

  enable(false);
  delay(250);

  dir(false);
  enable(true);
  
  pwm1.setPWM(STEP, 1024, 4096-1024); // 25% duty cycle, I think
  //pwm1.setPWM(STEP, 2048, 4096); // 50% duty cycle, I think

  delay(1000);
  enable(false);

  delay(1000);

 // delay(1000);
 // pwm1.setPWM(1, 2048, 4096); // 50% duty cycle, I think
 // pwm2.setPWM(1, 1024, 4096-1024); // 25% duty cycle, I think
}
