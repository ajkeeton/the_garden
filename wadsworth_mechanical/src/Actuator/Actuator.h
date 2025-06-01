#pragma once
#include <string>

#if 0
enum ActuatorState { IDLE, ACTUATOR_UP, ACTUATOR_DOWN };

class Actuator {
private:
    std::string name;
    int pwm;
    int dir1;
    int dir2;
    int limit_pin;

    // Movement state management
    ActuatorState state;
    unsigned long movement_start_time;
    unsigned long movement_duration;
    int current_speed;

public:
    Actuator(std::string name, int pwm, int dir1, int dir2, int limit_pin);
    ~Actuator();

    // Traditional direct control methods
    void move_up(int speed);
    void move_down(int speed);
    void stop();

    // Non-blocking control methods
    void start_moving_up(int speed, unsigned long duration_ms);
    void start_moving_down(int speed, unsigned long duration_ms);
    void reset(int speed = 300, unsigned long reset_duration_ms = 7000);

    // State management
    void update(); // Call this in each loop iteration
    bool is_idle() const;
    bool is_at_limit();
    ActuatorState get_state() const;

    // Event callbacks - to be set by client code if needed
    void (*on_movement_complete)(Actuator* actuator) = nullptr;
    void (*on_limit_reached)(Actuator* actuator) = nullptr;
};

extern Actuator* actuator_main;
extern Actuator* actuator_secondary;
#endif