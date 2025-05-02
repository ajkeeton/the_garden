#include "common.h"
#include "wifi.h"
#include "proto.h"
//#include <SimpleMDNS.h>

char ssid[] = "Shenanigans";
char pass[] = "catscatscats";
const char *gardener = "10.0.0.78";
//const char *gardener = "192.168.1.118";
int port = 7777;

void wifi_t::init(const char *n) {
  strcpy(name, n);
  init();
}

void wifi_t::init() {
  //Serial.printf("Initializing WiFi. mDNS name: %s\n", name);

  //WiFi.beginNoBlock(ssid, pass);
  //WiFi.setTimeout(10000);
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);

    // Perform a WiFi scan
    int nfound = WiFi.scanNetworks();
    if(nfound < 0) {
      Serial.println("WiFi scan failed");
    } 
    else if (!nfound ) {
      Serial.println("No WiFi networks found. But this hasn't been reliable at all...");
    } 
    else {
      Serial.printf("Found %d WiFi networks:\n", nfound);
      for (int i = 0; i < nfound; ++i) {
        Serial.printf("%d: %s (%d dBm)\n", i + 1, WiFi.SSID(i), WiFi.RSSI(i));
      }
    }
  }

  // Disable Nagle's so we can send small packets sooner
  client.setNoDelay(true); 

  //WiFi.setTimeout(100);
}

bool wifi_t::send(const char *buf, int len) {
  if(!client.connected()) {
    return false;
  }

  // Send the message
  if(client.write(buf, len) != len) {
    Serial.println("Failed to send message");
    client.stop();
    return false;
  }

  //Serial.printf("Sent %d: %d: %s\n", type, len, payload);
  return true;
}

bool wifi_t::recv() {
  if(!client.connected() || !client.available())
    return false;

  uint8_t header[6];
  int n = client.read(header, sizeof(header));
  if(n != sizeof(header)) {
    Serial.println("Failed to read message header.");
    client.stop();
    return false;
  }

  t_last_server_contact = millis();

  if(n < PROTO_HEADER_SIZE) {
    Serial.printf("Message length too small, dropping connection: %d < %d",
        n, PROTO_HEADER_SIZE);
    client.stop();
    return false;
  }

  uint16_t type = (header[0] << 8) | header[1];
  uint16_t version = (header[2] << 8) | header[3];
  uint16_t length = (header[4] << 8) | header[5];

  if(length > 1024) {
    Serial.printf("Message %d:%d:%d - length too large, dropping connection", 
        type, version, length);
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

  switch(type) {
    case PROTO_PING:
      Serial.println("Received ping. Sending identity");
    case PROTO_IDENT:
      send_ident();
      break;
    case PROTO_STATE_UPDATE:
      Serial.println("Received state update");
      queue_recv_state(payload, length);
      break;
    case PROTO_PULSE:
      Serial.println("Received pulse");
      queue_recv_pulse(payload, length);
      break;
    case PROTO_PIR_TRIGGERED:
      Serial.println("Received PIR triggered");
      queue_recv_pir(payload, length);
      break;
    // The above are the only messages we should ever receive
    default:
      Serial.printf("Received unknown message type: 0x%02X in:", type);
      for (int i = 0; i < 6; i++) {
        Serial.printf("%02X ", header[i]);
      }
      Serial.println();
      client.stop();
      return false;
  }

  return true;
}

void wifi_t::connect() {
  // No need to build up a queue if we're not connected
  uint32_t now = millis();

  if(now - last_retry < 2500) 
    return;

  last_retry = now;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi. Retrying");
    init();
    return;
  }

  mutex_enter_blocking(&mtx);
  if(msgq_send.size()) 
    msgq_send = {};
  mutex_exit(&mtx);

  Serial.printf("Trying Gardener at: %s:%u\n", gardener, port);  
  
  if(!client.connect(gardener, port)) {
    Serial.println("Failed to connect to Garden server");
    return;
  }

  t_last_server_contact = millis();

  send_ident();
}

void wifi_t::next() {  
  log_info();

  if(!client.connected()) {
    connect();
    return;
  }

  uint32_t now = millis();
  if(now - t_last_server_contact > TIMEOUT_SERVER) {
    Serial.println("Server timed out. Disconnecting");
    client.stop();
    return;
  }

  recv();
  send_pending();
}

void wifi_t::send_pending() {
  // Process the message queue
  // Just one at a time
  mutex_enter_blocking(&mtx);
  if(msgq_send.empty()) {
    mutex_exit(&mtx);
    return;
  }

  msg_t msg = msgq_send.front();
  
  msgq_send.pop();
  mutex_exit(&mtx);

  if(!send_msg(msg)) {
    Serial.println("Failed to send message from queue");
  }
  else {
    //Serial.printf("Sent message: Type=%d, Length=%d\n", 
    //    msg.get_type(), msg.get_length()); 
  }
}

void wifi_t::queue_send_push(const msg_t &msg) {
  if(!client.connected())
      return;

  mutex_enter_blocking(&mtx);
  if(msgq_send.size() > MAX_QUEUE) {
      Serial.println("Send queue is full, dropping first message");
      msgq_send.pop();
  }
  //else
  //    Serial.printf("send q size: %lu\n", msgq_send.size());
  msgq_send.push(msg);
  mutex_exit(&mtx);
}

// Send message struct
bool wifi_t::send_msg(const msg_t &msg) {
  return send(msg.buf, msg.full_len);
}

// Identify ourselves after connecting
void wifi_t::send_ident() {
  if(!client.connected())
      return;

  msg_t msg(name, WiFi.macAddress().c_str());
  // No need to queue, we only do this on first connect 
  // and we're not on the LED core
  if(!send_msg(msg)) {
      Serial.println("Failed to send ident message");
  }
}

// Send a pulse to the garden
// Currently using it for when a sensor is triggered and we want the rest 
// of the garden to respond
void wifi_t::send_pulse(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
  // Pulses need to be fast. Don't queue if not connected
  if(!client.connected())
      return;

  msg_t msg(color, fade, spread, delay);
  queue_send_push(msg);
}

void wifi_t::send_pir_triggered(uint16_t pir_index) {
  // Don't queue if not connected
  if(!client.connected())
      return;

  msg_t msg(pir_index);
  queue_send_push(msg);
}

// Sensor update message
void wifi_t::send_sensor_msg(uint16_t strip, uint16_t led, uint16_t percent, uint32_t age) {
  if(!client.connected())
      return;

  msg_t msg(strip, led, percent, age);
  queue_send_push(msg);
}

// Read the next queued message
// Intended for consumers
// Returns true if there was a message available and populates recv_msg_t
// recv_msg_t will have the message type set
bool wifi_t::recv_pop(recv_msg_t &msg) {
  mutex_enter_blocking(&mtx);
  if(msgq_recv.empty()) {
      mutex_exit(&mtx);
      return false;
  }

  msg = msgq_recv.front();
  msgq_recv.pop();
  mutex_exit(&mtx);
  return true;
}

void wifi_t::log_info() {
  uint32_t now = millis();

  if(now - last_log < 2500)
    return;

  last_log = now;

  Serial.printf("IP|MAC: %s | %s. Gardener: %s. Connected: %d\n", 
    WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), 
    gardener, client.connected());
}