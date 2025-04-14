#pragma once

// Interface for input multiplexer
//
// During operation I noticed that getting a reliable read required some time 
// after changing mux addresses. A few micros seemed to do the trick
// 
// THUS:
//  In order for this wrapper to work you have to continuously call next()

#define MUX_EN 22
#define MUX4 20
#define MUX3 19
#define MUX2 18
#define MUX1 17

#define MUX_IN1 A2

class Mux_Read {
public:
  int vals[16];
  int idx = 0;
  uint32_t last = micros(), 
           delay = 100; // microseconds to wait before next read

  Mux_Read() {    
    pinMode(MUX_EN, OUTPUT);
    pinMode(MUX4, OUTPUT);
    pinMode(MUX3, OUTPUT);
    pinMode(MUX2, OUTPUT);
    pinMode(MUX1, OUTPUT);
    pinMode(MUX_IN1, INPUT);

    for(int i=0; i<16; i++)
      vals[i] = 0;

    set_address(0);
  }

  void init() {
    // Populate all inputs
    do {
        next();
    } while(idx != 0);
  }

  // Sets address for next read
  void set_address(int pin) {
    digitalWrite(MUX_EN, HIGH); // Not 100% certain if switching on/off matters
    digitalWrite(MUX1, pin & 1);
    digitalWrite(MUX2, pin & 2);
    digitalWrite(MUX3, pin & 4);
    digitalWrite(MUX4, pin & 8);
    digitalWrite(MUX_EN, LOW);
  }

  void next() {
    uint32_t now = micros();
    if(now - last < delay)
      return;
    last = now;

    vals[idx] = analogRead(MUX_IN1);

    //Serial.printf("Mux %d: %d\n", idx, vals[idx]);
    idx++;
    if(idx > 15)
      idx = 0;

    set_address(idx);
  }

  bool read_switch(int pin) {
    next();
    return vals[pin] > 128;
  }

  uint32_t read_raw(int pin) {
    return vals[pin];
  }
};
