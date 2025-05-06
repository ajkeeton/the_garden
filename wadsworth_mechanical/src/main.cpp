#include "Actuator/Actuator.h"
#include <Arduino.h>

// Pin definitions
const int PWM1 = 0;
const int DIR1 = 1;
const int DIR2 = 2;

const int PWM2 = 3;
const int DIR3 = 6;
const int DIR4 = 7;

// const int STICK1 = A0;
// const int STICK2 = A1;
const int STOP_MAIN_PIN = A0;
const int STOP_SECONDARY_PIN = A1;

char printBuffer[40];

// Setup global for actuators, initialized in setup()
Actuator* actuator_main = nullptr;
Actuator* actuator_secondary = nullptr;

// Movement settings
const unsigned long pause_duration = 150; // Pause between direction changes

// State variables for the cycling logic
enum CycleState { START_UP, MOVING_UP, PAUSE_BEFORE_DOWN, START_DOWN, MOVING_DOWN, PAUSE_BEFORE_UP };

CycleState main_cycle_state = START_UP;
CycleState secondary_cycle_state = START_UP;
unsigned long main_state_change_time = 0;
unsigned long secondary_state_change_time = 0;

// Forward declarations for callbacks
void on_main_complete(Actuator* actuator);
void on_secondary_complete(Actuator* actuator);
void on_main_limit(Actuator* actuator);
void on_secondary_limit(Actuator* actuator);

void setup()
{
    // Initialize hardware
    analogWriteFreq(1024);

    pinMode(PWM1, OUTPUT);
    pinMode(DIR1, OUTPUT);
    pinMode(DIR2, OUTPUT);
    pinMode(PWM2, OUTPUT);
    pinMode(DIR3, OUTPUT);
    pinMode(DIR4, OUTPUT);
    pinMode(STOP_MAIN_PIN, INPUT);
    pinMode(STOP_SECONDARY_PIN, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(DIR1, 0);
    digitalWrite(DIR2, 0);
    digitalWrite(DIR3, 0);
    digitalWrite(DIR4, 0);

    Serial.begin(9600);
    delay(100);

    // Initialize Actuators
    actuator_main = new Actuator("main", PWM1, DIR1, DIR2, STOP_MAIN_PIN);
    actuator_secondary = new Actuator("secondary", PWM2, DIR3, DIR4, STOP_SECONDARY_PIN);

    // Set callbacks for movement completion and limit reached
    actuator_main->on_movement_complete = on_main_complete;
    actuator_main->on_limit_reached = on_main_limit;
    actuator_secondary->on_movement_complete = on_secondary_complete;
    actuator_secondary->on_limit_reached = on_secondary_limit;

    // Reset actuators to starting bottom position
    actuator_main->move_down(150);
    actuator_secondary->move_down(150);
    delay(7000);
    actuator_main->stop();
    actuator_secondary->stop();

    Serial.println("Starting...");

    // Initialize the cycle state machine
    main_cycle_state = START_UP;
    secondary_cycle_state = START_UP;
    main_state_change_time = millis();
    secondary_state_change_time = millis();
}

/** Callback functions **/
void on_main_complete(Actuator* actuator)
{
    Serial.println("Main actuator movement completed");
    // Any specific actions when main actuator completes movement
}

void on_secondary_complete(Actuator* actuator)
{
    Serial.println("Secondary actuator movement completed");
    // Any specific actions when secondary actuator completes movement
}

void on_main_limit(Actuator* actuator)
{
    Serial.println("Main actuator reached limit");
    // Reset the actuator by moving it down for 7000ms at speed 300
    actuator->reset(300, 7000);
}

void on_secondary_limit(Actuator* actuator)
{
    Serial.println("Secondary actuator reached limit");
    // Reset the actuator by moving it down for 7000ms at speed 300
    actuator->reset(300, 7000);
}
/** End Callback Functions */

// Example cycle pattern with state handling
void cycle(int speed, int duration, Actuator* actuator, CycleState& cycle_state, unsigned long& state_change_time)
{
    unsigned long current_time = millis();
    bool idle = actuator->is_idle();

    switch (cycle_state) {
    case START_UP:
        actuator->start_moving_up(speed, duration);
        cycle_state = MOVING_UP;
        state_change_time = current_time;
        break;

    case MOVING_UP:
        // Wait for actuator to complete its movement
        if (idle) {
            cycle_state = PAUSE_BEFORE_DOWN;
            state_change_time = current_time;
        }
        break;

    case PAUSE_BEFORE_DOWN:
        // Small pause before changing direction
        if (current_time - state_change_time >= pause_duration) {
            cycle_state = START_DOWN;
            state_change_time = current_time;
        }
        break;

    case START_DOWN:
        actuator->start_moving_down(speed, duration);
        cycle_state = MOVING_DOWN;
        state_change_time = current_time;
        break;

    case MOVING_DOWN:
        // Wait for actuator to complete its movement
        if (idle) {
            cycle_state = PAUSE_BEFORE_UP;
            state_change_time = current_time;
        }
        break;

    case PAUSE_BEFORE_UP:
        // Small pause before starting the cycle again
        if (current_time - state_change_time >= pause_duration) {
            cycle_state = START_UP;
            state_change_time = current_time;
        }
        break;
    }
}

void loop()
{
    // Update each actuator (checks limits, handles timing)
    actuator_main->update();
    actuator_secondary->update();

    cycle(200, 5000, actuator_main, main_cycle_state, main_state_change_time);
    cycle(200, 3000, actuator_secondary, secondary_cycle_state, secondary_state_change_time);
}