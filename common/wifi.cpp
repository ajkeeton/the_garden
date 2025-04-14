#include "common.h"
#include "wifi.h"
#include <LEAmDNS.h> // Include the LEAmDNS library
#include <WiFiClient.h> // Include WiFiClient for TCP communication

char ssid[] = "Shenanigans";
char pass[] = "catscatscats";

WiFiMulti wf;
MDNSResponder mdns; // Create an instance of the LEAmDNS responder
WiFiClient client;  // Create a WiFiClient for TCP communication

void Wifi::init() {
  wf.addAP(ssid, pass); 

  if (wf.run() != WL_CONNECTED) 
    return;

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize mDNS
  if (!mdns.begin("venus")) { // Start mDNS responder with the device's IP
    Serial.println("Error starting mDNS");
    return;
  }

  Serial.println("mDNS responder started");

  retry_in = 0;
}

bool Wifi::discover_server() {
  // Use mDNS to discover GardenServer._garden._tcp.local
  if(!mdns.queryService("_garden", "_tcp")) {
    Serial.println("No GardenServer found");
    port = 0;
    garden_server = IPAddress(0); 
    return false;
  }

  garden_server = mdns.IP(0);
  port = mdns.port(0);

  Serial.printf("GardenServer: %s:%u\n", garden_server.toString().c_str(), port);

  return true;
}

bool Wifi::send_msg(uint16_t type, uint16_t len, char *payload) {
  if(!client.connected()) {
    if(!discover_server())
      return false;

    Serial.println("Connecting to GardenServer...");
    if(!client.connect(garden_server, port)) {
      Serial.println("Failed to connect to GardenServer");
      return false;
    }
  }

  // Format the message
  uint8_t message[6 + len];
  int version = 1;
  message[0] = (type >> 8) & 0xFF;
  message[1] = type & 0xFF;
  message[2] = (version >> 8) & 0xFF;
  message[3] = version & 0xFF;
  message[4] = (len >> 8) & 0xFF;
  message[5] = len & 0xFF;
  memcpy(&message[6], payload, len);

  // Send the message
  if(client.write(message, sizeof(message)) != sizeof(message)) {
    Serial.println("Failed to send message");
    client.stop();
    return false;
  }

  Serial.printf("Sent %d: %d: %s\n", type, len, payload);
  return true;
}

bool Wifi::send_msg(uint16_t type, const String &payload) {
  return send_msg(type, payload.length(), (char*)payload.c_str());
}

bool Wifi::send_sensor_msg(uint16_t type, uint16_t index, uint16_t value) {
  char payload[32];
  snprintf(payload, sizeof(payload), "%u,%u", index, value);
  return send_msg(type, strlen(payload), payload);
}

bool Wifi::read_msg() {
  if(!client.connected()) {
    Serial.println("Client not connected, cannot read messages.");
    return false;
  }

  if(!client.available())
    return false;

  uint8_t header[6];
  int n = client.read(header, sizeof(header));
  if(n != sizeof(header)) {
    Serial.println("Failed to read message header.");
    client.stop();
    return false;
  }

  uint16_t type = (header[0] << 8) | header[1];
  uint16_t version = (header[2] << 8) | header[3];
  uint16_t length = (header[4] << 8) | header[5];

  switch(type) {
    case 1:
      Serial.println("Received echo");
      break;
    case 4:
      Serial.println("Received state update");
      break;
    default:
      Serial.printf("Received unknown message type: %d (%x)\n", type, type);
      client.stop();
      return false;
  }

  if(length > 1024) {
    Serial.printf("Message %d:%d:%d - length too large, dropping connection.", type, version, length);
    client.stop();
    return false;
  }

  char payload[length + 1];
  n = client.readBytes(payload, length);
  if(n != length) {
    Serial.println("Failed to read message payload.");
    client.stop();
    return false;
  }
  payload[length] = 0;

  Serial.printf("Received: Type=%d, Version=%d, Length=%d, Payload=%s\n", 
      type, version, length, payload);

  return true;
}

void Wifi::run() {
  uint32_t now = millis();

  if (wf.run() != WL_CONNECTED) {
    if (retry_in && retry_in > now)
      return;

    retry_in = now + 2000;
    Serial.println("Trying to connect to wifi");
    init();
  } else {
    if (now - last_log > 5000) {
      Serial.printf("Our IP: ");
      Serial.println(WiFi.localIP());
      last_log = now;

      send_msg(1, "Echo?!");
      send_msg(2, "Log!");
      send_sensor_msg(3, 0, 10);

      while (read_msg()) {
        // Keep reading messages until no more are available?
      }
    }
  }
}

