
#include <Servo.h>
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

#define SERVOMIN  100 // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  500 // This is the 'maximum' pulse length count (out of 4096)
//#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
//#define USMAX  2400 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

Servo servos[3];

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(9600);

  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  pwm1.begin(); 
  pwm2.begin();

  pwm1.setOscillatorFrequency(27000000);
  pwm1.setPWMFreq(SERVO_FREQ);

  delay(10);
  Serial.println("Starting...");
}

int angle_to_pwm(int angle) {
  angle = map(angle, 0, 180, SERVOMIN, SERVOMAX);
  int analog_value = int(float(angle) / 1000000 * SERVO_FREQ * 4096);
  return analog_value;
}

void loop() {
  Serial.println("Loop");

  Serial.println("Setting mid");
  pwm1.setPWM(8, 0, (SERVOMAX-SERVOMIN)/2);
  pwm1.setPWM(9, 0, (SERVOMAX-SERVOMIN)/2);
  pwm1.setPWM(10, 0, (SERVOMAX-SERVOMIN)/2);

  delay(500);

  Serial.println("Setting max*.75");
  pwm1.setPWM(8, 0, SERVOMAX*.75);
  pwm1.setPWM(9, 0, SERVOMAX*.75);
  pwm1.setPWM(10, 0, SERVOMAX*.75);

  delay(500);
  Serial.println("Setting min*1.25");

  pwm1.setPWM(8, 0, SERVOMIN*1.25);
  pwm1.setPWM(9, 0, SERVOMIN*1.25);
  pwm1.setPWM(10, 0, SERVOMIN*1.25);
  delay(500);

  //pwm1.setPWM(8, 0, 2048);
  //pwm1.setPWM(9, 0, 2048);
  //pwm1.setPWM(10, 0, 2048);

  #if 0
  pwm1.setPWM(8, 0, angle_to_pwm(90));
  pwm1.setPWM(9, 0, angle_to_pwm(90));
  pwm1.setPWM(10, 0, angle_to_pwm(90));

  delay(500);
  pwm1.setPWM(8, 0, angle_to_pwm(180));
  pwm1.setPWM(9, 0, angle_to_pwm(180));
  pwm1.setPWM(10, 0, angle_to_pwm(180));

  delay(500);
  pwm1.setPWM(8, 0, angle_to_pwm(0));
  pwm1.setPWM(9, 0, angle_to_pwm(0));
  pwm1.setPWM(10, 0, angle_to_pwm(0));

  delay(500);
  #endif

}
