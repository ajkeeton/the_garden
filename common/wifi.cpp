
#include "common.h"
#include "wifi.h"

char ssid[] = "741-g";
char pass[] = "ilovekittens";

WiFiMulti wf;

void Wifi::init() {
  wf.addAP(ssid, pass); 

  if (wf.run() != WL_CONNECTED) 
    return;

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  retry_in = 0;
}

void Wifi::run() {
  uint32_t now = millis();


  if (wf.run() != WL_CONNECTED) {
    uint32_t now = millis();

    if(retry_in && retry_in > now)
        return;

    retry_in = now + 2000;
    Serial.println("Trying to connect to wifi");
    init();
  }
  else {  
    if(now - last_log > 10000) {
      Serial.printf("IP address: ");
      Serial.println(WiFi.localIP());
      last_log = now;

      // XXX print last message and-or ping from hub
    }
  }
}

