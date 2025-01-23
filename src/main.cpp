#include "Adafruit_VL53L0X.h"
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
#include <WiFi.h>
#include <Time.h> 
#include "ESPAsyncWebServer.h"
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>


// todo fonction prototypes 
//void update();

bool SOFTAP = true;  // true : AP mode final deployment, false : client mode for development

const char* ssid = "ssid";
const char* password = "wifipw";
const char *ssid_AP = "esp32tof";
const char *password_AP = "password";

VL53L0X_RangingMeasurementData_t measure;
//déclarer le tableau T_distance
const int itmax = 1000;
double T_distance[itmax] ;
int it = 0;
double Tsamp = 0.02;
double Tsamp_us = 20000 ;
hw_timer_t *timer = NULL;

// Create AsyncWebServer object on port 80
IPAddress ip(192, 168, 1, 30); // where xx is the desired IP Address
IPAddress gateway(192, 168, 1, 1); // set gateway to match your network
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const uint32_t SERIAL_SPEED = 115200; ///< Set the baud rate for Serial I/O
// port I2C pour afficheur (optionel)
#define OLED_SDA 33
#define OLED_SCL 32
#define SDA 23
#define SCL 19
//#define OLED_SDA 23
//#define OLED_SCL 19
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);   // (rotation, [reset [, clock, data]])

volatile struct { //! DEPECATED
    unsigned Running: 1;
    unsigned DisplayConsole: 1;
    unsigned Update: 1;
    unsigned notifyWSclient: 1;
    unsigned KeyPressed: 1;
} Flags;

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

float dist;
float temps;

int Score;
#define KeyNone 0
#define KeyRight 1
#define KeyLeft 2
#define KeyDown 3
#define KeyRotateRight 4
#define KeyRotateLeft 5
#define KeyPause 6
int KeyPressed = 0;

// Pour configurer une interruption Timer
//hw_timer_t *timer = NULL;
//const double Tsamp_us = 200000;  // 200 ms = 200000 us
const double dt = 200e-3;  // 200 ms (same as Tsamp_us but in s)

// Compteur logiciel
const int DispNcount = 2;    // 0.40 s
const int notifyNcount = 2;  // 0.400 s
volatile int Dispcount = 0;
volatile int notifycount = 0;

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    // Serial.println((char*) data);
    if (strcmp((char*)data, "toggle") == 0) {
      Flags.Running = !Flags.Running;
      ws.textAll("run "+String(Flags.Running));
    }
    if (strcmp((char*)data, "R") == 0) {
      KeyPressed = KeyRight;
      Flags.KeyPressed = 1;
//      ws.textAll("R");
    }
    if (strcmp((char*)data, "L") == 0) {
      KeyPressed = KeyLeft;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "D") == 0) {
      KeyPressed = KeyDown;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "RR") == 0) {
      KeyPressed = KeyRotateRight;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "RL") == 0) {
      KeyPressed = KeyRotateLeft;
      Flags.KeyPressed = 1;
    }
    if (strcmp((char*)data, "P") == 0) {
      KeyPressed = KeyPause;
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

String processor(const String& var){ //! to be adapted
  Serial.println(var);
  if(var == "STATE"){ // non encore utilisé ici
    if (Flags.Running){
      return "Running";
    }
    else{
      return "Stopped";
    }
  }
  else   if(var == "Score"){
    return String(Score);
  }

  return String();
}

void notifyWSclient() {
    String timeAndDistance = "temps " + String(temps, 2) + " distance " + String(dist, 2);
    ws.textAll(timeAndDistance);
}

String generateCSV() {
    String csv = "Time (s),Distance (mm)\n";
    for (int i = 0; i < it; i++) {
        // Skip uninitialized values
        if (T_distance[i] < 0) continue;
        csv += String(Tsamp * i, 2) + "," + String(T_distance[i], 2) + "\n";
    }
    return csv;
}

//----------------------------------------------------------------
void IRAM_ATTR onTimer() { // ici on est toutes les Tsamp = 200 ms
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
    char S[50]; //50 chars should be enough
    strftime(S, 50, "%d/%m/%Y %H:%M:%S", &timeinfo);
    String SS = String(S);
    return (SS);
}

String getFile() {
    char data[200];
    int i, il;
    const int ds = 5; // 1 pt sur 5 donc 100 ms
    String page = "Distance data\n";
    // page += "DTH: ";
    if (!SOFTAP) page += getFormattedDateTime();
    page += "\n\nParameteres\n";

    page += "t,T_distance\n";  // Affiche seulement le temps et la distance
    for (int i = it + 1; i < it + 1 + itmax; i += 5) {  // ds = 5 (100 ms)
        int il = i;
        if (il > itmax) il -= itmax;
        sprintf(data, "%.2f,%.2f\n", Tsamp * (i - (it + 1)), T_distance[i]);  // Affiche le temps et la distance
        page += data;
    }
    return page;
}
String sendData() {
    char data[200];
    sprintf(data, "{\"distance\":%.2f}",
            measure.RangeMilliMeter);
//    sprintf(data, "{\"Press\":%.2f,\"Umot\":%.2f,\"Vol\":%.2f,\"bPress\":%.2f,\"Temp\":%.2f,\"Humid\":%.2f,\"Run\":%d}",
//    Press, Umot, Vol, BMP280_Pressure, Si7021_temp, Si7021_humid, Flags.Running);
    Serial.println(data);
    return data;
}

String sendDataArray() { // retourne string avec donnée ++ écrit fichier json sur data
    char data[100];
    int i;
    const int ds = 5; // 1 pt sur 5 donc 100 ms
    String resp = "{\"distance\":[";
    sprintf(data, "{\"distance\":[");
    for (i = 0; i < itmax; i += ds) {
        sprintf(data, "%.2f,", T_distance[it]);
        resp += data;
    }
    resp.setCharAt(resp.length() - 1, ']');
    resp += "}";
//  Serial.println(resp);
    return resp;
}

void initVar() {
  Flags.Running = 1;  // Starts On, should be Off
  Flags.Update = 0;
  Flags.notifyWSclient = 0;
  Flags.DisplayConsole = 1;   // starts On : debug
  Dispcount = 0;
  notifycount = 0;
  Score = 0;
}

void setup() {
  Serial.begin(SERIAL_SPEED);

  while (!Serial) {
    delay(1);
  }

  Wire.begin(SDA, SCL);

  Serial.println("VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1);
  }
// power
  Serial.println(F("VL53L0X API Simple Ranging example\n\n"));
  
  /* pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);  // OFF */ // ? no pin required

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  initVar();
  // Connect to Wi-Fi
  if (SOFTAP) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid_AP, password_AP);
    Serial.println("");
    Serial.print("Wifi Server: ");
    Serial.print("AP Wifi Server: ");
    Serial.println(ssid_AP);
    Serial.print("PW: ");
    Serial.println(password_AP);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    delay(2000);
  } else {
    IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
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
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  server.on("/data.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/csv", generateCSV());
  });
  // route to send ARRAY of Press, Umot, Vol data
  server.on("/dataarray.json", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", sendDataArray());  // envoie les Datas 1000 pts
  });
  // route to send Press, Umot, Vol data
  server.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", sendData());  // envoie les Datas 1 pt
  });
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println();
  

  // config timer0 et installation de l'interruption
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, Tsamp_us, true);
  timerAlarmEnable(timer);
}

void loop() {
    VL53L0X_RangingMeasurementData_t measure;

    lox.rangingTest(&measure, false); // Perform measurement

    temps += dt; // Increment time

    if (measure.RangeStatus != 4) {
        dist = measure.RangeMilliMeter;
    } else {
        dist = -1; // Out of range
    }

    T_distance[it] = dist; // Store distance in array
    it = (it + 1) % itmax; // Circular buffer

    // Print time and distance to Serial Monitor
    Serial.print("Time (s): ");
    Serial.print(temps, 2); // Print time with 2 decimal places
    Serial.print(", Distance (mm): ");
    if (dist == -1) {
        Serial.println("Out of Range");
    } else {
        Serial.println(dist, 2); // Print distance with 2 decimal places
    }

    notifyWSclient(); // Notify WebSocket clients
    delay(100); // Delay for measurement
}