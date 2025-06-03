#pragma once 
#include <cstdint>

#define WADS_V2A_BOARD

#define DEF_MUX_PIN_POT_1 0
#define DEF_MUX_PIN_POT_2 1

#define DEF_MUX_PIN_LIMIT_1 2
#define DEF_MUX_PIN_LIMIT_2 3

#define DEF_POT_AT_MIN 80
#define DEF_POT_AT_MAX 450

#define POT_TARGET_WAKE (DEF_POT_AT_MAX * .8)
#define DEF_SPEED 255

enum STATE_T {
    STATE_WAKEUP = 0,
    STATE_IDLE = 1,
    STATE_SIGH_START = 2, // Start a sigh
    STATE_SIGH_END = 3, // End of sigh
    STATE_BREATHE_SHALLOW = 4, // Shallow breathing
    STATE_BREATHE = 5, // Breathing
    STATE_REST = 6, // Return
    STATE_TEST_CYCLE = 8, // Test cycle 
    STATE_MOVE_TO_TGT = 9, // Move to arbitrary target position 
    STATE_SLEEP = 10
};

class behavior_t {
public:
    uint32_t t_last_log = 0;
    uint16_t target = 0;

    behavior_t() {}
    void init();
    void step();
    void log_periodic();
    void do_wakeup();
};

class actuator_t {
public:
    const char* name = nullptr;
    STATE_T state = STATE_WAKEUP,
            state_next = STATE_REST;
    int pattern_idx = 0; // Index of the current pattern
    int pin_pwm = 0,
        pin_dir1 = 0,
        pin_dir2 = 0,
        pin_pot = 0, 
        pin_limit = 0;
    uint8_t speed = 0;
    bool dir_up = false; // true = up, false = down
    int16_t pot_target = 0,
            pot_at_start = DEF_POT_AT_MIN, // This value will change based on VCC+the v divider. Need to determine by timing
            pot_at_end = DEF_POT_AT_MAX; // This value will change based on VCC+the v divider

    uint32_t t_last_log = 0,
             t_last_move = 0,
             t_last_stop = 0, // Used to prevent flapping
             t_delay_until = 0; // Delay before move to cooldown

    actuator_t() {}
    void init(const char *n, int pot, int limit, int pwm, int dir1, int dir2);
    void set_direction_up(bool up);
    void set_direction_up(bool up, int s);
    void step();
    void stop();
    void end(); // Stop and reset the actuator to the current pot position
    void handle_button();
    void handle_sensor(uint16_t sens);
    void log_periodic();

    bool near_target_stop(); // stop when we're close
    bool near_target_start(); // don't start unless we're slightly further away than the stop point

    // void do_wakeup();
    void do_test_cycle();
    void do_move_to_target();
    void do_rest();
    void do_expand();
    void do_idle();

    void init_next(); // We've reached our target, transition to the next state
    void init_wakeup();
    void init_sigh();
    void init_sigh_end();
    void init_test_cycle();

    void sleepy_time();

    uint16_t midpoint() {
        return (DEF_POT_AT_MIN + DEF_POT_AT_MAX) / 2;
    }
    void handle_state_update(uint16_t idx, uint16_t score) {
        pattern_idx = idx;
        switch(pattern_idx) {
            case 0:
                Serial.println("Pattern 0: Sleepy time");
                break;
            case 1:
                Serial.println("Pattern 1: Wakeup. Shallow breathing");
                break;
            case 2:
                Serial.println("Pattern 2: Normal breathing");
                break;
            default:
                pattern_idx = 1;
                Serial.println("Unknown pattern, defaulting to pattern 1");
                break;
        }
    }
};