
#include "common.h"

char ssid[] = "Mycelial";
char pass[] = "Maybe a cult";

WiFiMulti wf;

void Wifi::init() {
  wf.addAP(ssid, pass); 

  if (wf.run() != WL_CONNECTED) 
    return;

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  retry_in = 0;
}

void Wifi::run() {

  if (wf.run() != WL_CONNECTED) {
    uint32_t now = millis();

    if(retry_in && retry_in < now)
        return;

    retry_in = now + 1000;
    Serial.println("Trying to connect to wifi");
    init();
  }
}

