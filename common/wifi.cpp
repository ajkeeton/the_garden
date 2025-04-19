#include "common.h"
#include "wifi.h"
#include "proto.h"
#include <LEAmDNS.h> // Include the LEAmDNS library

char ssid[] = "Shenanigans";
char pass[] = "catscatscats";

// WiFiMulti wf;
MDNSResponder mdns; // Create an instance of the LEAmDNS responder

void wifi_t::init(const char *n) {
  strcpy(name, n);

  Serial.printf("Initializing WiFi. mDNS name: %s\n", name);

  WiFi.beginNoBlock(ssid, pass);
  WiFi.setTimeout(100);

  if(!mdns.begin(name)) { 
    Serial.println("Error starting mDNS");
    return;
  }

  init();
}

void wifi_t::init() {
  if(WiFi.status() != WL_CONNECTED) {
    return;
  }
#if 0
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize mDNS
  if (!mdns.begin(name)) { 
    Serial.println("Error starting mDNS");
    return;
  }

  Serial.println("mDNS responder started");
#endif
  retry_in = 0;
}

bool wifi_t::discover_server() {
  // Use mDNS to discover GardenServer._garden._tcp.local
  if(!mdns.queryService("_garden", "_tcp")) {
    Serial.println("No GardenServer found");
    port = 0;
    garden_server = IPAddress(0); 
    return false;
  }

  garden_server = mdns.IP(0);
  port = mdns.port(0);

  Serial.printf("Found Garden server: %s:%u\n", garden_server.toString().c_str(), port);

  if(!client.connect(garden_server, port)) {
    Serial.println("Failed to connect to Garden server");
    return false;
  }
  return true;
}

bool wifi_t::send_msg(const char *buf, int len) {
  // Send the message
  if(client.write(buf, len) != len) {
    Serial.println("Failed to send message");
    client.stop();
    return false;
  }

  //Serial.printf("Sent %d: %d: %s\n", type, len, payload);
  return true;
}

#if 0
bool wifi_t::send_msg(uint16_t type, uint16_t len, char *payload) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  #warning wrap up connection + discovery to include the mutex
  if(!client.connected()) {
    if(!discover_server()) {
      xSemaphoreGive(mtx);
      return false;
    }
  }
  xSemaphoreGive(mtx);

  // Format the message
  uint8_t message[6 + len];
  message[0] = (type >> 8) & 0xFF;
  message[1] = type & 0xFF;
  message[2] = (PROTO_VERSION >> 8) & 0xFF;
  message[3] = PROTO_VERSION & 0xFF;
  message[4] = (len >> 8) & 0xFF;
  message[5] = len & 0xFF;
  memcpy(&message[6], payload, len);

  // Writes shooooould be thread safe...

  // Send the message
  if(client.write(message, sizeof(message)) != sizeof(message)) {
    Serial.println("Failed to send message");
    client.stop();
    return false;
  }

  Serial.printf("Sent %d: %d: %s\n", type, len, payload);
  return true;
}
#endif

bool wifi_t::read_msg() {
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
    case PROTO_PING:
      Serial.println("Received ping");
      break;
    case PROTO_STATE_UPDATE:
      Serial.println("Received state update");
      break;
    // The above are the only messages we should ever receive
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

void wifi_t::run() {
  uint32_t now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if(WiFi.localIP().isSet()) {
      // We were once connected but are no longer. Need to restart WiFi and mDNS:
      mdns.close();
      
      WiFi.beginNoBlock(ssid, pass);

      if(!mdns.begin(name)) { 
        Serial.println("Error starting mDNS");
        return;
      }

      WiFi.localIP().clear();
    }

    if (retry_in && retry_in > now)
      return;

    retry_in = now + 2000;
    Serial.println("Trying to connect to wifi");
    init();
    return;
  }
  
  if (now - last_log > 5000) {
    //if(!garden_server.isSet() || !client.connected())
    if(!client.connected())
      discover_server();

    last_log = now;
    Serial.printf("IP|MAC: %s | %s. Gardener: %s\n", 
        WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), 
        garden_server.isSet() ? garden_server.toString().c_str() : "[not found]");

    #if 0
    // Test connection
    if(client.connected()) {
      send_msg(PROTO_PING, "PING?!");
      send_msg(PROTO_LOG, "Log!");
      send_sensor_msg(3, 0, 10);

      //while (read_msg()) {
        // Keep reading messages until no more are available?
    
    }
    #endif
  }

  if(!client.connected()) {
    // No need to build up a queue if we're not connected
    msgq = {};
    return;
  }

  // Process the message queue
  if(!msgq.empty()) {
    xSemaphoreTake(mtx, portMAX_DELAY);
    msg_t msg = msgq.front();
    msgq.pop();
    xSemaphoreGive(mtx);

    if(!send_msg(msg)) {
    //if(!send_msg(msg.type, msg.len, (char*)msg.payload)) {
      Serial.println("Failed to send message from queue");
    }
    else {
      Serial.printf("Sent message: Type=%d, Length=%d, Payload=%s\n", 
          msg.get_type(), msg.get_length(), msg.get_payload()); 
    }
  }

  while (read_msg()) {
    // Keep reading messages until no more are available?
  }
}

