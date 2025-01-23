#include "Adafruit_VL53L0X.h"
#include <WiFi.h>
#include <Time.h>
#include "ESPAsyncWebServer.h"
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

//? Constants
const char* ssid = "ssid";
const char* password = "wifipw";
const char *ssid_AP = "esp32tof";
const char *password_AP = "password";
const uint32_t SERIAL_SPEED = 115200;
const int itmax = 1000;
const double Tsamp = 0.02;
const double Tsamp_us = 20000;
const double dt = 200e-3;
const int DispNcount = 2;
const int notifyNcount = 2;
const int ds = 5; // 1 pt sur 5 donc 100 ms

//? Variables
bool SOFTAP = true;
double T_distance[itmax];
int it = 0;
float dist;
float temps;
int Score;
int KeyPressed = 0;
volatile struct {
  unsigned Running: 1;
  unsigned DisplayConsole: 1;
  unsigned Update: 1;
  unsigned notifyWSclient: 1;
  unsigned KeyPressed: 1;
} Flags;
volatile int Dispcount = 0;
volatile int notifycount = 0;
hw_timer_t *timer = NULL;

//? Objects
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
VL53L0X_RangingMeasurementData_t measure;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//*---------------------------------------------------------------

void APWifiParam() {
  Serial.println("Empty wifi Param from SPIFFS, use AP :");
  ssid = ssid_AP;
  password = password_AP;
  SOFTAP = true;
  Serial.print("AP Wifi Server: ");
  Serial.println(ssid);
  Serial.print("PW: ");
  Serial.println(password);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      Flags.Running = !Flags.Running;
      ws.textAll("run " + String(Flags.Running));
    }
    if (strcmp((char*)data, "R") == 0) {
      KeyPressed = 1;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "L") == 0) {
      KeyPressed = 2;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "D") == 0) {
      KeyPressed = 3;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "RR") == 0) {
      KeyPressed = 4;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "RL") == 0) {
      KeyPressed = 5;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "P") == 0) {
      KeyPressed = 6;
      Flags.KeyPressed = 1;
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void notifyWSclient() {
  String timeAndDistance = "temps " + String(temps, 2) + " distance " + String(dist, 2);
  ws.textAll(timeAndDistance);
}

String generateCSV() {
  String csv = "Time (s),Distance (mm)\n";
  for (int i = 0; i < it; i++) {
    if (T_distance[i] < 0) continue;
    csv += String(Tsamp * i, 2) + "," + String(T_distance[i], 2) + "\n";
  }
  return csv;
}

void IRAM_ATTR onTimer() {
  Flags.Update = 1;
  if (++Dispcount >= DispNcount) {
    Dispcount = 0;
    Flags.DisplayConsole = 1;
  }
  if (++notifycount >= notifyNcount) {
    notifycount = 0;
    Flags.notifyWSclient = 1;
  }
}

String getFormattedDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "NA";
  }
  char S[50];
  strftime(S, 50, "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(S);
}

String sendData() {
  char data[200];
  sprintf(data, "{\"distance\":%.2f}", measure.RangeMilliMeter);
  Serial.println(data);
  return data;
}

String sendDataArray() {
  String resp = "{\"distance\":[";
  for (int i = 0; i < itmax; i += ds) {
    resp += String(T_distance[it], 2) + ",";
  }
  resp.setCharAt(resp.length() - 1, ']');
  resp += "}";
  return resp;
}

void initVar() {
  Flags.Running = 1;
  Flags.Update = 0;
  Flags.notifyWSclient = 0;
  Flags.DisplayConsole = 1;
  Dispcount = 0;
  notifycount = 0;
}

//*---------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_SPEED);
  while (!Serial) {
    delay(1);
  }
  Wire.begin(23, 19);
  Serial.println("VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1);
  }
  Serial.println(F("VL53L0X API Simple Ranging example\n\n"));
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  initVar();
  if (SOFTAP) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid_AP, password_AP);
    Serial.println("");
    Serial.print("AP Wifi Server: ");
    Serial.println(ssid_AP);
    Serial.print("PW: ");
    Serial.println(password_AP);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    delay(2000);
  } else {
    IPAddress ip(192, 168, 1, 30);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns1(8, 8, 8, 8);
    IPAddress dns2(8, 8, 4, 4);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.config(ip, gateway, subnet, dns1, dns2);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi..");
    }
    Serial.println(WiFi.localIP());
    delay(2000);
  }
  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String());
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){ // Serve the CSS file
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){ // Serve the JavaScript file
    request->send(SPIFFS, "/script.js", "application/javascript");
  });
  server.on("/data.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/csv", generateCSV());
  });
  server.on("/dataarray.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", sendDataArray());
  });
  server.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", sendData());
  });
  server.begin();
  Serial.println("HTTP server started");
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, Tsamp_us, true);
  timerAlarmEnable(timer);
}

void loop() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  temps += dt;
  dist = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
  T_distance[it] = dist;
  it = (it + 1) % itmax;
  Serial.print("Time (s): ");
  Serial.print(temps, 2);
  Serial.print(", Distance (mm): ");
  if (dist == -1) {
    Serial.println("Out of Range");
  } else {
    Serial.println(dist, 2);
  }
  notifyWSclient();
  delay(100);
}
