#include "sensors.h"
#include "common/wifi.h"

extern wifi_t wifi;

sensors_t::sensors_t() {
    mux.init();

    for(int i=0; i<MAX_MUX_IN; i++) {
        //map_strip_led[i] = 0; // strip 0, LED 0
        pir_map[i] = false;
        sensor_to_led_map[i].init();
    }
}

void sensors_t::add(uint16_t sensor, uint16_t strip, uint16_t led) {
    if(sensor >= MAX_MUX_IN) {
        Serial.printf("Sensor %u is out of range, max is %d\n", sensor, MAX_MUX_IN);
        return;
    }

    //map_strip_led[sensor] = ((uint32_t)(strip) << 16) | led;
    sensor_to_led_map[sensor].add(sensor, strip, led);
}

void sensors_t::add_pir(uint16_t mux_pin) {
    if(mux_pin >= MAX_MUX_IN) {
        Serial.printf("Sensor %u is out of range, max is %d\n", mux_pin, MAX_MUX_IN);
        return;
    }
    pir_map[mux_pin] = true;
}

void sensors_t::init(int pin_start, int pin_end, 
        void (*start)(int, int, const sensor_state_t &),
        void (*is)(int, int, const sensor_state_t &),
        void (*off)(int, int, const sensor_state_t &),
        void (*pir)(int)
    ) {

    sens_start = pin_start;
    sens_end = pin_end;
    on_trigger_start = start;
    on_is_triggered = is;
    on_trigger_off = off;
    on_pir = pir;

    // XXX It's intentionally just pin_end. Technically we'd want pin_end - 
    // pin_start AND an offset.
    mux.num_inputs = pin_end;
}

void sensors_t::do_on_trigger_start(int i) {
    //Serial.printf("State on start: sensor %d: %lu, percent: %u\n", 
    //    i, sensors[i].value, sensors[i].percent());

    sensor_to_led_map_t &s = sensor_to_led_map[i];

    for(int i=0; i<s.nshared; i++) {
        Serial.printf("Trigger start: %d, %d: %u\n", s.strip[i], s.led[i], sensors[i].percent());
        on_trigger_start(s.strip[i], s.led[i], sensors[s.sens_index]);
    }
}

void sensors_t::do_is_triggered(int i) {
    //Serial.printf("State is triggered: %d: %lu > %lu percent: %u\n", 
    //    i, sensors[i].value, sensors[i].minmax.get_thold(), sensors[i].percent());
    sensor_to_led_map_t &s = sensor_to_led_map[i];
    for(int i=0; i<s.nshared; i++) {
        on_is_triggered(s.strip[i], s.led[i], sensors[s.sens_index]);
    }
}

void sensors_t::do_on_trigger_off(int i) {
    sensor_to_led_map_t &s = sensor_to_led_map[i];
    for(int i=0; i<s.nshared; i++) {
        on_trigger_off(s.strip[i], s.led[i], sensors[s.sens_index]);
    }
}

void sensors_t::next() {
    uint32_t now = millis();
    
    // Need to periodically tell the server about the sensor state 
    // regardless of triggers so it can detect daytime
    bool send_update = false;
    if(now - t_last_sensor_update_sent > T_REMOTE_SENSOR_UPDATE_DELAY) {
        t_last_sensor_update_sent = now;
        send_update = true;
    }

    for(int i=sens_start; i<sens_end; i++) {
        mux.next();
        uint16_t raw = mux.read_raw(i);

        if(pir_map[i]) {
            // With the 14.6V tolerant sensor boards, a triggered PIR comes out to about 180
            if(raw >= 150) {
                // PIR is triggered. Hit the callback to update the global State
                on_pir(i);
            }
            continue;
        }

        sensor_state_t &sensor = sensors[i];
        bool was_triggered = sensor.t_trigger_start > 0;
        sensor.update(raw);
        bool is_triggered = sensor.t_trigger_start > 0;

        //if(is_triggered)
        //        Serial.printf("Sensor %d triggered: value: %u, duration: %u, minmax: %u/%u/%u\n", 
        //            i, sensor.value, sensor.age(), 
        //            sensor.minmax.get_min(), sensor.minmax.get_max(), sensor.minmax.get_thold());

        // If we were not triggered, but are now, call trigger_start
        if(!was_triggered) {
            if(is_triggered) {
                do_on_trigger_start(i);
                send_update = true;
            }
        }
        else {
            // We were triggered
            // Check if we still are
            if(!is_triggered)
                do_on_trigger_off(i);
            // We were triggered and still are,
            else
                do_is_triggered(i);
        }

        if(send_update) {
            // Serial.printf("Sending sensor update: %d\n", i);
            wifi.send_sensor_msg(
                sensor_to_led_map[i].strip[0], 
                sensor_to_led_map[i].led[0],
                sensor.percent(), 
                raw);
        }
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
    Serial.printf("Button pressed, triggering\n");
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
    #if 0
    int i=0; 
    Serial.printf("Sensor %d: value: %u, age: %.03fs, minmax: %u/%u/%u\n", 
        i, sensors[i].value, (float)(sensors[i].age())/1000, 
        sensors[i].minmax.get_min(), 
        sensors[i].minmax.get_max(), 
        sensors[i].minmax.get_thold());
    #endif
 
    //int i = 4;
    // Serial.printf("Sensor %d: value: %u, percent: %u, age: %.03fs\n", 
    //    i, sensors[i].value, sensors[i].percent(), (float)(sensors[i].age())/1000);
    //sensors[i].minmax.log_info();
    Serial.printf("Sensors:\n");
    for(int i=sens_start; i<sens_end; i++) {
        sensor_state_t &s = sensors[i];
        Serial.printf("%d: %u|%u|%u\t", i, mux.read_raw(i), s.value, s.minmax.get_thold());

        //if(sensors[i].value) {
        //    Serial.printf("Sensor %d: value: %u, percent: %u, age: %.03fs\n", 
        //        i, sensors[i].value, sensors[i].percent(), (float)(sensors[i].age())/1000);
            s.minmax.log_info();
        //}
    }
}