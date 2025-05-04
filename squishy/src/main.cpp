
#include <Arduino.h>
#include "common/wifi.h"

const char *a_name_for_the_garden_server = "squish";

wifi_t wifi;

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        delay(10);
    }

    wifi.init(a_name_for_the_garden_server);
}

void loop() {
    // WiFi stuff has to be handled on core 0
    // Handle connection, send/recv, etc
    wifi.next();
    wifi.log_info();
}

void loop1() {
    // Everythig else I put on core 1
    Serial.println("Core 1 doing stuff...");

    recv_msg_t msg;
    while(wifi.recv_pop(msg)) {
        switch(msg.type) {
            case PROTO_PULSE:
                Serial.printf("The gardener told us to pulse the LEDs: %u %u %u %u\n", 
                    msg.pulse.color, msg.pulse.fade, msg.pulse.spread, msg.pulse.delay);
                break;
            case PROTO_STATE_UPDATE:
                Serial.printf("A state update message: %u %u\n", 
                        msg.state.state_idx, msg.state.score);
                break;
            case PROTO_PIR_TRIGGERED:
                Serial.println("A PIR sensor was triggered");
                break;
            default:
                Serial.printf("Unknown message type: %u\n", msg.type);
        }
    }

    delay(500); 
}