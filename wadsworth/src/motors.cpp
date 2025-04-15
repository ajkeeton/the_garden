#include <Arduino.h>

const int PWM1 = 0;
const int DIR1 = 1;
const int DIR2 = 2;

// const int PWM2 = 3;
// const int DIR3 = 4;
// const int DIR4 = 5;

const int STICK1 = A0;
const int STICK2 = A1;

char printBuffer[40];

// const int POS1 = A2;
// const int POS2 = A3;

void set_forward(const uint16_t pin1, const uint16_t pin2)
{
    Serial.println("set_forward");
    digitalWrite(pin1, 1);
    digitalWrite(pin2, 0);
}

void set_backward(const uint16_t pin1, const uint16_t pin2)
{
    Serial.println("set_backward");
    digitalWrite(pin1, 0);
    digitalWrite(pin2, 1);
}

uint16_t get_pos(uint8_t pin) { return map(analogRead(pin), 0, 1024, 0, 100); }

void handleReadStick(const uint16_t stick, const uint16_t pwm, const uint16_t dir1, const uint16_t dir2)
{
    // put your main code here, to run repeatedly:
    // static int newx = 0, lastx = 0;
    int x = analogRead(stick);
    // XXX necessary per docs to have 100ms delay when changing directions
    static bool was_forward = false;

    if (abs(x - 512) < 50) {
        analogWrite(pwm, 0);
        digitalWrite(LED_BUILTIN, 0);
        return;
    }

    digitalWrite(LED_BUILTIN, 1);

    // newx = x - lastx;
    //  if(abs(newx) > 10) {
    //    lastx = x;
    //  }

    if (x > 512) {
        if (!was_forward) {
            // XXX necessary per docs to have 100ms delay when changing directions
            delay(150);
        }
        was_forward = true;
        set_forward(dir1, dir2);
    } else {
        if (was_forward) {
            // XXX necessary per docs to have 100ms delay when changing directions
            delay(150);
        }
        was_forward = false;
        set_backward(dir1, dir2);
    }

    uint16_t speed = map(abs(x - 512), 0, 512, 0, 255);
    snprintf(printBuffer, sizeof printBuffer, "%d: %d - %d, %d\n", pwm, speed, x, was_forward);
    Serial.println(printBuffer);
    Serial.println("Pos: ");
    analogWrite(pwm, speed);
}

void cycleDirection(int speed, int delayTime)
{
    analogWrite(PWM1, speed);
    set_forward(DIR1, DIR2);
    delay(delayTime);
    set_backward(DIR1, DIR2);
    delay(delayTime);
}

void motors_setup()
{
    // put your setup code here, to run once:
    analogWriteFreq(1000);

    pinMode(PWM1, OUTPUT);
    pinMode(DIR1, OUTPUT);
    pinMode(DIR2, OUTPUT);
    // pinMode(PWM2, OUTPUT);
    // pinMode(DIR3, OUTPUT);
    // pinMode(DIR4, OUTPUT);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(DIR1, 0);
    digitalWrite(DIR2, 0);
    // digitalWrite(DIR3, 0);
    // digitalWrite(DIR4, 0);

    Serial.begin(9600);
    delay(100);

    // Reset Back to start
    set_backward(DIR1, DIR2);
    analogWrite(PWM1, 75);
    delay(10000);

    Serial.println("Starting...");
}

void motors_loop()
{
    // handleReadStick(STICK1, PWM1, DIR1, DIR2);
    cycleDirection(100, 3000);
}
