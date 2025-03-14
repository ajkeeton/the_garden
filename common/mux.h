#pragma once

#define MUX_EN 22
#define MUX4 20
#define MUX3 19
#define MUX2 18
#define MUX1 17

#define MUX_IN1 28

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
