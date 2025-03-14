#include "common.h"

extern Adafruit_PWMServoDriver servos;

class servo_t {
public:
  int state = 0;
  int pin = 0;
  int last = 0;

  void init(int p) {
    pin = p;
    state = random() % 3;
    last = millis() + random(1000);
  }

  void zero() {
    Serial.printf("Servo %d: zero'ing for %d\n", pin);
    servos.setPWM(pin, 0, SERVOMIN);
  }

  void mid() {
    Serial.printf("Servo %d: setting mid\n", pin);
    uint16_t mid = (SERVOMAX-SERVOMIN)/2 + SERVOMIN;
    servos.setPWM(pin, 0, mid);
  }

  void max() {
    Serial.printf("Servo %d: setting max\n", pin);
    servos.setPWM(pin, 0, SERVOMAX);
  }

  void update() {
    uint32_t now = millis();

    if(now < last || (now - last) < SERVO_UPDATE_DELTA)
      return;

    last = now;
      
    switch(state++) {
      case 0:
        zero();
        break;
      case 1:
        mid();
        break;
      case 2:
        max();
        break;
      default:
        state = 0;
        last += random(1000);
    }
  }
};

servo_t servo_state[3];

void servos_init() {
  servos.begin();
  servos.setOscillatorFrequency(26000000);
  servos.setPWMFreq(SERVO_FREQ);

  servo_state[0].init(0);
  servo_state[1].init(1);
  servo_state[2].init(2);
}

bool do_zero() {
  //return !false;
  return mux_read_switch(DIP_0);
}

void do_servos() {
  for(int i=0; i<sizeof(servo_state)/sizeof(servo_state[0]); i++) {
    if(do_zero())
      servo_state[i].zero();
    else
      servo_state[i].update();
  }
}