#include "sensors.h"

sensors_t::sensors_t() {
    mux.init();

    for(int i=0; i<MAX_MUX_IN; i++) {
        map_strip_led[i] = 0; // strip 0, LED 0
    }
}

void sensors_t::add(uint16_t sensor, uint16_t strip, uint16_t led) {
    if(sensor >= MAX_MUX_IN) {
        Serial.printf("Sensor %u is out of range, max is %d\n", sensor, MAX_MUX_IN);
        return;
    }

    map_strip_led[sensor] = ((uint32_t)(strip) << 16) | led;
}

void sensors_t::init(int pin_start, int pin_end, 
        void (*start)(int, int, const sensor_state_t &),
        void (*is)(int, int, const sensor_state_t &),
        void (*off)(int, int, const sensor_state_t &)) {

    sens_start = pin_start;
    sens_end = pin_end;
    on_trigger_start = start;
    on_is_triggered = is;
    on_trigger_off = off;
}

void sensors_t::do_on_trigger_start(int i) {
    //Serial.printf("Sensor %d start\n", i);
    int sidx = (map_strip_led[i] >> 16) & 0xFFFF; // Strip index
    int lidx = map_strip_led[i] & 0xFFFF; // LED index
    on_trigger_start(sidx, lidx, sensors[i]);

    Serial.printf("State: sensor %d: on start: %lu\n", i, sensors[i].value);
}

void sensors_t::do_is_triggered(int i) {
    int sidx = (map_strip_led[i] >> 16) & 0xFFFF; // Strip index
    int lidx = map_strip_led[i] & 0xFFFF; // LED index
    on_is_triggered(sidx, lidx, sensors[i]);

    //Serial.printf("State: %d: on start: %lu, %.3fs\n", i, s.value, (float)(s.age())/1000);
}

void sensors_t::do_on_trigger_off(int i) {
    int sidx = (map_strip_led[i] >> 16) & 0xFFFF; // Strip index
    int lidx = map_strip_led[i] & 0xFFFF; // LED index
    on_trigger_off(sidx, lidx, sensors[i]);
}

void sensors_t::next() {
    mux.next();

    for(int i=sens_start; i<sens_end; i++) {
        bool was_triggered = sensors[i].t_trigger_start > 0;
        uint16_t raw = mux.read_raw(i);
        //bool is_triggered = sensors[i].update(raw);
        sensors[i].update(raw);
        bool is_triggered = sensors[i].t_trigger_start > 0;

        // If we were not triggered, but are now, call trigger_start
        if(!was_triggered) {
            if(is_triggered)
                do_on_trigger_start(i);
            return;
        }
        
        // We were triggered. Check if we still are
        if(!is_triggered)
            do_on_trigger_off(i);

        // We were triggered and still are,
        else
            do_is_triggered(i);
    }
}

// Pretend the first sensor is triggered
void sensors_t::button_pressed() {
    bool was_trigged = sensors[0].t_trigger_start > 0;
    sensors[0].update(1024);
    if(!was_trigged)
        do_on_trigger_start(0);
    else
        do_is_triggered(0);
    // Serial.printf("Button pressed: %lu\n", sensors[0].t_trigger_start);
}

void sensors_t::button_hold() {
    button_pressed();
}

void sensors_t::log_info() {
    #if 0
    for(int i=sens_start; i<sens_end; i++) {
        if(sensors[i].value) {
            Serial.printf("Sensor %d: value: %u, duration: %u, minmax: %u/%u/%u\n", 
                i, sensors[i].value, sensors[i].duration, 
                sensors[i].minmax.get_min(), sensors[i].minmax.get_max(), sensors[i].minmax.get_thold());
        }
    }
    #endif
    int i=0;
    Serial.printf("Sensor %d: value: %u, age: %.03fs, minmax: %u/%u/%u\n", 
        i, sensors[i].value, (float)(sensors[i].age())/1000, 
        sensors[i].minmax.get_min(), 
        sensors[i].minmax.get_max(), 
        sensors[i].minmax.get_thold());
}