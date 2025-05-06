#include "Actuator/Actuator.h"
#include <Arduino.h>
#include <iostream>
#include <string>

#define LIMIT_THRESHOLD 1022

Actuator::Actuator(std::string name, int pwm, int dir1, int dir2, int limit_pin)
    : name(name)
    , pwm(pwm)
    , dir1(dir1)
    , dir2(dir2)
    , limit_pin(limit_pin)
    , state(IDLE)
    , movement_start_time(0)
    , movement_duration(0)
    , current_speed(0)
{
    std::cout << "constructing " << pwm << '\n';
}

Actuator::~Actuator() { std::cout << "destructing " << pwm << '\n'; }

void Actuator::move_up(int speed)
{
    Serial.printf("move up %s \n", name.c_str());
    digitalWrite(dir1, 0);
    digitalWrite(dir2, 1);
    analogWrite(pwm, speed);
    current_speed = speed;
    state = ACTUATOR_UP;
}

void Actuator::move_down(int speed)
{
    Serial.printf("move down %s \n", name.c_str());
    digitalWrite(dir1, 1);
    digitalWrite(dir2, 0);
    analogWrite(pwm, speed);
    current_speed = speed;
    state = ACTUATOR_DOWN;
}

void Actuator::stop()
{
    Serial.printf("stop %s \n", name.c_str());
    digitalWrite(dir1, 0);
    digitalWrite(dir2, 0);
    analogWrite(pwm, 0);
    current_speed = 0;
    state = IDLE;
}

void Actuator::start_moving_up(int speed, unsigned long duration_ms)
{
    if (is_at_limit()) {
        Serial.printf("%s already at limit, not moving up\n", name.c_str());
        stop();
        return;
    }

    move_up(speed);
    movement_start_time = millis();
    movement_duration = duration_ms;
}

void Actuator::start_moving_down(int speed, unsigned long duration_ms)
{
    move_down(speed);
    movement_start_time = millis();
    movement_duration = duration_ms;
}

bool Actuator::is_at_limit()
{
    // Take average of multiple readings. Software solution for handling noise on the ADC pins.
    // Could also add a 1uf capacitor between the limit switch pin and ground to filter noise.
    const int numSamples = 5;
    int sum = 0;
    for (int i = 0; i < numSamples; i++) {
        sum += analogRead(limit_pin);
        delayMicroseconds(100); // Small delay between readings
    }
    int average = sum / numSamples;

    if (average > LIMIT_THRESHOLD) {
        Serial.printf("%s Limit Switch %d \n", name.c_str(), average);
    }
    return average > LIMIT_THRESHOLD;
}

bool Actuator::is_idle() const { return state == IDLE; }

ActuatorState Actuator::get_state() const { return state; }

void Actuator::update()
{
    // Serial.printf("Limit Switch %d \n", analogRead(limit_pin));
    // Check for limit switch
    if (state == ACTUATOR_UP && is_at_limit()) {
        Serial.printf("%s limit reached while moving up\n", name.c_str());
        stop();

        // Call callback if set
        if (on_limit_reached != nullptr) {
            on_limit_reached(this);
        }
        return;
    }

    // Check for movement completion based on time
    if (state != IDLE) {
        unsigned long current_time = millis();
        if (current_time - movement_start_time >= movement_duration) {
            Serial.printf("%s movement completed after %lu ms\n", name.c_str(), current_time - movement_start_time);
            stop();

            // Call callback if set
            if (on_movement_complete != nullptr) {
                on_movement_complete(this);
            }
        }
    }
}
void Actuator::reset(int speed, unsigned long reset_duration_ms)
{
    Serial.printf("Resetting %s - moving down for %lu ms at speed %d\n", name.c_str(), reset_duration_ms, speed);
    move_down(speed);
    movement_start_time = millis();
    movement_duration = reset_duration_ms;
}