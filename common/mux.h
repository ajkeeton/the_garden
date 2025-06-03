#pragma once

// Interface for input multiplexer
//
// During operation I noticed that getting a reliable read required some time 
// after changing mux addresses. A few micros seemed to do the trick
// 
// THUS:
//  In order for this wrapper to work you have to continuously call next()

#ifdef WADS_V1_BOARD
#define MUX_IN1 A2
#define MUX_EN 22
#define MUX4 20
#define MUX3 19
#define MUX2 18
#define MUX1 17
#else
#define MUX_IN1 A0
#define MUX_EN 18
#define MUX4 22
#define MUX3 21
#define MUX2 20
#define MUX1 19
#endif

class mux_t {
public:
  int vals[16];
  int num_inputs = 16; // Number of inputs on the mux
  int idx = 0;
  uint32_t last = 0,
           t_read_delay = 20; // microseconds to settle before next analogRead

  int n_avg = 0;

  mux_t() {    
    pinMode(MUX_EN, OUTPUT);
    pinMode(MUX4, OUTPUT);
    pinMode(MUX3, OUTPUT);
    pinMode(MUX2, OUTPUT);
    pinMode(MUX1, OUTPUT);
    pinMode(MUX_IN1, INPUT);

    for(int i=0; i<16; i++)
      vals[i] = 0;

    set_address(0);

    init();
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
    if(now - last < t_read_delay)
      return;
    last = now;

    uint16_t raw = analogRead(MUX_IN1);
    if(n_avg > 0) {
      // Aprox average the readings
      vals[idx] = (vals[idx] * n_avg + raw) / (n_avg + 1);
    } else {
      vals[idx] = raw;
    }


    if(++idx >= 5)
      idx = 0;

    // Change address for next read
    set_address(idx);
  }

  bool read_switch(int pin) {
    //next();
    return vals[pin] > 512;
  }

  uint32_t read_raw(int pin) {
    if(pin < 0 || pin >= 16) {
      Serial.printf("\nXXX\nmux_t::read_raw: Invalid pin %d\nXXX\n", pin);
      return 0;
    }

    return vals[pin];
  }

  void log_info() {
    Serial.print("Mux: ");
    for (int i = 0; i < num_inputs; i++) {
      Serial.printf("%3d", vals[i]);
      if (i < num_inputs-1) {
        Serial.print(" | ");
      }
    }
    Serial.println();
  }
};
