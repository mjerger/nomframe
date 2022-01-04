/* 
 *  16*16 pixel led wifi interface
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <base64.h>

const char *ssid = "2rw";
const char *password = "Handschuhfachbeleuchtungsschalter";
const char *mdnsHostname = "nomframe";

const char* methods[8] { "ANY", "GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS" };

ESP8266WebServer server(80);

class CRGB {
 public:
  uint8_t r;
  uint8_t g;
  uint8_t b;
  CRGB() {}
  CRGB(uint8_t r, uint8_t g, uint8_t b) {
    this->r = r;
    this->g = g;
    this->b = b;
  }
};

bool readyToSend = true;

#define NUM_LEDS 50
CRGB leds[NUM_LEDS];

#define REQ_PIN 5 //D1
#define CLK_PIN 4 //D2
#define DAT_PIN 0 //D3

#define LED_DELAY 300


void errorLoop() {
  while (true) {
    for (int i=0; i<3; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
    delay(1000);
  }
}


String getContentType(String filename)
{
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}



// Send file from FS back
bool handleFile()
{  
  String path = server.uri();
  
  if(path.endsWith("/")) path += "index.htm";
  
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  // Send requested (compressed) file
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    
    if(SPIFFS.exists(pathWithGz)) path += ".gz";
    
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    
    return true;
  }
  
  return false;
}


bool handleREST()
{
  if (!server.uri().startsWith("/r/")) return false;
  
  String path = server.uri().substring(3);
  HTTPMethod method = server.method();

  DynamicJsonDocument json(2048);
  
  if (path == "list")
  {  
    if (method == HTTP_GET) {
      // TODO read all animations
      json["hello"] = "world";
      return sendJson(json);
    }
  } else if (path.startsWith("ani/")) {
    int pos = path.lastIndexOf("/");
    String name = path.substring(pos+1);
    if (name.length() > 0) {
      json["name"] = name;
      return sendJson(json);
    }
  }

  return false;
}

bool sendJson(DynamicJsonDocument json)
{
  String string;
  serializeJson(json, string);
  server.send(200, "application/json", string);
  return true;
}


void handleLEDs()
{
  if (digitalRead(REQ_PIN) == LOW && readyToSend) {
    static uint8_t clk = 0;
    static uint8_t color = 0;

    digitalWrite(CLK_PIN, clk);  
    for (uint32_t l=0; l<NUM_LEDS; l++) {
      color = leds[l].r;
      for (uint8_t i=0; i<8; i++) {
        digitalWrite(DAT_PIN, (color >> i) & 1);
        clk = 1-clk;
        for (uint32_t i=0; i<LED_DELAY; i++) {__asm__("nop\n\t"); }
        digitalWrite(CLK_PIN, clk);
        for (uint32_t i=0; i<LED_DELAY; i++) {__asm__("nop\n\t"); }
      }
      color = leds[l].g;
      for (uint8_t i=0; i<8; i++) {
        digitalWrite(DAT_PIN, (color >> i) & 1);
        clk = 1-clk;
        for (uint32_t i=0; i<LED_DELAY; i++) {__asm__("nop\n\t"); }
        digitalWrite(CLK_PIN, clk);
        for (uint32_t i=0; i<LED_DELAY; i++) {__asm__("nop\n\t"); }
      }
      color = leds[l].b;
      for (uint8_t i=0; i<8; i++) {
        digitalWrite(DAT_PIN, (color >> i) & 1);
        clk = 1-clk;
        for (uint32_t i=0; i<LED_DELAY; i++) {__asm__("nop\n\t"); }
        digitalWrite(CLK_PIN, clk);
        for (uint32_t i=0; i<LED_DELAY; i++) {__asm__("nop\n\t"); }
      }
    }
  }
}

void setup(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  pinMode(REQ_PIN, INPUT_PULLUP);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DAT_PIN, OUTPUT);

  Serial.begin(9600);
  delay(500);
  Serial.println("\n\n--------------");
  Serial.println(" nomframe 1.0");
  Serial.println("--------------");

  Serial.print("Initializing SPIFFS ... ");
  if(SPIFFS.begin()) {
    Serial.print("ok");
  } else {
    Serial.println("failed");
    errorLoop();
  }

  // Connect to WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(" ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ok");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start MSDN responder
  if (MDNS.begin("nomframe")) {
    Serial.println("MDNS responder started: nomframe.local");
  }

  // Handle requests
  server.onNotFound([]() {
    Serial.print(methods[server.method()]);
    Serial.print(" " + server.uri() + " ... ");
    
    if (handleREST() || handleFile()) {
      Serial.println("ok");
    } else {
      Serial.println("not found");
      server.send(404, "text/plain", "404: Not Found");
    }
  });
  
  server.begin();
  Serial.println("HTTP server started");


  for (int i=0; i<NUM_LEDS; i++) {
    leds[i] = CRGB(0xAA,0xAA,0xAA);
  }
  leds[0] = CRGB(255,0,0);
  leds[1] = CRGB(0,255,0);
  leds[2] = CRGB(0,0,255);
}

void loop(void)
{
  server.handleClient();
  MDNS.update();
  handleLEDs();
}
