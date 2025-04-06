#include <WiFi.h>
#include <WebSocketsClient.h>

//const char *ssid = "741-g";
//const char *password = "ilovekittens";

const char *ssid = "Shenanigans";
const char *password = "catscatscats";
const char *host = "10.0.0.78";
const uint16_t port = 3000;

int status = WL_IDLE_STATUS;
WiFiMulti wifi;
WebSocketsClient webSocket;

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.println("[WSc] Connected!");

      /*
      webSocket.sendTXT("{\"type\":\"hello\",\"data\":{\"message\":\"hello from pico\"}}");
      {
        "type": "hello",
        "data": {
          "macAddress": "00:1A:2B:3C:4D:5E",
          "metadata": {
            "deviceName": "SensorNode1",
            "location": "Lab1"
          }
        }
      }
      */
      
      break;
    case WStype_TEXT:
      Serial.print("[WSc] get text:");
      Serial.println((char *)payload);

      // send message to server
      // webSocket.sendTXT("message here");
      break;
    case WStype_BIN:
      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_PING:
    case WStype_PONG:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void setup() {
  Serial.begin(9600);

  while (!Serial) { }

  Serial.printf("\n\n\n");

  Serial.print("Connecting to ");
  Serial.println(ssid);

  wifi.addAP(ssid, password);

  if (wifi.run() != WL_CONNECTED) {
    Serial.println("Unable to connect to network, rebooting in 10 seconds...");
    delay(10000);
    rp2040.reboot();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Connected to WiFi");

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // server address, port and URL
  webSocket.begin(host, port);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();

  delay(500);
  Serial.println("Sending next....");

  char msg[256];
  sprintf(msg, 
  "{"\
    "\"type\": \"log\","\
    "\"data\": {"\
      "\"message\": \"%lu: log message from a pico\""\
      "}"\
    "}", millis());

  webSocket.sendTXT(msg);

  sprintf(msg, 
  "{"\
    "\"type\": \"sensor\","\
    "\"data\": {"\
    "  \"value\": %lu"\
    "}"\
  "}", random(0, 1024));

  webSocket.sendTXT(msg);

  sprintf(msg, 
  "{"\
    "\"type\": \"stepper\","\
    "\"data\": {"\
    "  \"index\": 0,"\
    "  \"state\": {"\
    "    \"position\": %lu,"\
    "    \"delay\": 50,"\
    "    \"acceleration\": 10,"\
    "    \"position_end\": 200,"\
    "    \"position_to_decel\": 150"\
    "  }"\
  "}}", random(0, 9000));

  webSocket.sendTXT(msg);
}
