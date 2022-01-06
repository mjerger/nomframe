

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
#include <uptime.h>
#include <uptime_formatter.h>
#include <time.h>

// WIFI HTTP server

const char *ssid = "2rw";
const char *password = "Handschuhfachbeleuchtungsschalter";
const char *mdnsHostname = "nomframe";
const String methods[8] { "ANY", "GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS" };
ESP8266WebServer server(80);

// NTP

#define MY_NTP_SERVER "at.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"   
time_t now;
tm tm;
const String weekdays[7] { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// LED strip interface

struct CRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  CRGB() : r(0), g(0), b(0){}
  CRGB(uint8_t all) : r(all), g(all), b(all) {}
  CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
  String toHex() {
    String cr = String(r, HEX);
    String cg = String(g, HEX);
    String cb = String(b, HEX);

    return (cr.length() == 1 ? "0" + cr : cr) + 
           (cg.length() == 1 ? "0" + cg : cg) + 
           (cb.length() == 1 ? "0" + cb : cb);
  }
};

#define NUM_LEDS 120
CRGB leds[NUM_LEDS];

#define REQ_PIN 5 //D1
#define CLK_PIN 4 //D2
#define DAT_PIN 0 //D3

#define LED_DELAY_PRE  100
#define LED_DELAY_POST 150


// Loop and blink forever
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


// The content types we serve
String getContentType(String filename)
{
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js"))  return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".png")) return "image/png";
  return "text/plain";
}


// Send file from FS back
bool handleFile(String path)
{  
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

  DynamicJsonDocument json(10000);
  
  if (path == "list") 
  {  
    if (method == HTTP_GET) {
      // TODO list /animations directory and generate json
      json["hello"] = "world";
      return sendJson(json);
    }
  } else if (path == String("sysinfo"))
  {
    // Uptime
    json["uptime"] = uptime_formatter::getUptime();

    // NTP time
    time(&now);
    localtime_r(&now, &tm);
    json["datetime"] = weekdays[tm.tm_wday]      + " " +
                       leadingZero(tm.tm_mday)   + "." + 
                       leadingZero(tm.tm_mon+1)  + "." + 
                       String(tm.tm_year + 1900) + " " + 
                       leadingZero(tm.tm_hour)   + ":" + 
                       leadingZero(tm.tm_min)    + ":" + 
                       leadingZero(tm.tm_sec);
    
    // Network
    json["ssid"] = String(ssid);
    json["host"] = String(mdnsHostname) + ".local";
    json["ip"]   = WiFi.localIP();
    
    // Filesystem
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    json["fs_free"]  = String(double(fs_info.totalBytes - fs_info.usedBytes) / 1024.0, 1) + "k";
    json["fs_used"]  = String((double)fs_info.usedBytes / 1024.0, 1) + "k";
    json["fs_total"] = String((double)fs_info.totalBytes / 1024.0, 1) + "k";
    json["fs_blockSize"]     = fs_info.blockSize;
    json["fs_pageSize"]      = fs_info.pageSize;
    json["fs_maxOpenFiles"]  = fs_info.maxOpenFiles;
    json["fs_maxPathLength"] = fs_info.maxPathLength;

    // LEDs
    String colors;
    for (int l=0; l<NUM_LEDS; l++) {
      colors = colors + leds[l].toHex();
    }
    json["leds"] = colors;
    
    return sendJson(json);
  }

  return false;
}

String leadingZero(int num)
{
  if (num < 10) return "0" + String(num);
  return String(num);
}

bool sendJson(DynamicJsonDocument json)
{
  String string;
  serializeJson(json, string);
  server.send(200, "application/json", string);
  return true;
}


uint8_t flip = 0;

void handleLEDs()
{
  if (digitalRead(REQ_PIN) == LOW) {
    static uint8_t clk = 0;
    static uint8_t color = 0;

    digitalWrite(CLK_PIN, clk);
    for (uint32_t b=0; b<NUM_LEDS*3; b++) {
      
      color = ((uint8_t*)&leds)[b];
      for (uint8_t i=0; i<8; i++) {
        digitalWrite(DAT_PIN, (color >> i) & 1);
        clk = 1-clk;
        
        for (uint32_t i=0; i<LED_DELAY_PRE; i++) __asm__("nop\n\t");
        
        digitalWrite(CLK_PIN, clk);
        
        for (uint32_t i=0; i<LED_DELAY_POST; i++) __asm__("nop\n\t");
      }
    }

    leds[3] = CRGB(flip);
    leds[4] = CRGB(flip < 128 ? 255 : 0);
    if (flip < 255) flip++;
    else flip=0;
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
  if (MDNS.begin(mdnsHostname)) {
    Serial.println("MDNS responder started: " + String(mdnsHostname) + ".local");
  }

  // Handle requests
  server.on("/",                  HTTP_GET, []() { handleFile ("/home.html"); } );
  server.on("/animations",        HTTP_GET, []() { handleFile ("/animations.html"); } );
  server.on("/animations/create", HTTP_GET, []() { handleFile ("/animations-edit.html"); } );
  server.on("/patterns",          HTTP_GET, []() { handleFile ("/patterns.html"); } );
  server.on("/patterns/create",   HTTP_GET, []() { handleFile ("/patterns-edit.html"); } );
  
  server.onNotFound([]() {
    Serial.print(methods[server.method()]);
    Serial.print(" " + server.uri() + " ... ");
    
    if (handleREST() || handleFile(server.uri())) {
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

  // TODO prepare next frame here
  handleLEDs();
}
