

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
#include <Lua.h>

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

struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  RGB() : r(0), g(0), b(0){}
  RGB(uint8_t all) : r(all), g(all), b(all) {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
  String toHex() {
    String cr = String(r, HEX);
    String cg = String(g, HEX);
    String cb = String(b, HEX);

    return (cr.length() == 1 ? "0" + cr : cr) + 
           (cg.length() == 1 ? "0" + cg : cg) + 
           (cb.length() == 1 ? "0" + cb : cb);
  }
};

#define FPS 60
uint32_t t;

#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS (WIDTH * HEIGHT)
RGB leds[NUM_LEDS];
#define REQ_PIN 5 //D1 - request LED data

bool playRandom = true;
uint16_t playDuration = 100;
uint16_t playTime = 0;

struct Pattern {
  uint8_t numFrames;
  uint8_t* data;
  uint8_t currentFrame;
  uint8_t fps;
};

String playingAnimation;
Pattern playingPattern;
uint8_t playingPatternFrame;

Lua lua;

////////////////////////////////////////////////////////////////////////////////

bool playAnimation(String animation)
{
  String path = (animation.startsWith("/a/") ? animation : "/a/" + animation) + ".lua";

  if (SPIFFS.exists(path) == false) {
    return false;
  }

  File file = SPIFFS.open(path, "r");
  playingAnimation = "";
  while (file.available()) {
    playingAnimation += file.readStringUntil('\n');
  }
}


bool animate(uint32_t dt)
{
  if (playingAnimation.length() > 0)
  {
    String script = "leds = {";
    for (uint16_t l=0; l<NUM_LEDS;l++) {
      if (l != 0) script += ",";
      script += "{" + String(leds[l].r) + "," + String(leds[l].g) + "," + String(leds[l].b) + "}";
    }
    script += "}\r\ndt = " + String(dt) + "\r\n" + playingAnimation;
    String ret = lua.execute(&script);
    Serial.println(ret);
  }
  return false;
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


// Handle the /r rest endpoints
bool handleREST()
{
  if (!server.uri().startsWith("/r/")) return false;
  
  String path = server.uri().substring(3);
  HTTPMethod method = server.method();

  DynamicJsonDocument json(2000);

// FILES - list all files
  if (path == "files") 
  {  
    if (method == HTTP_GET) {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        json[dir.fileName()] = dir.fileSize();
      }
      return sendJson(json);
    }
  }
// ANIMATIONS - list all animations
  else if (path == "animations") 
  {  
    if (method == HTTP_GET) {
      Dir dir = SPIFFS.openDir("/a/");
      while (dir.next()) {
        json[dir.fileName().substring(3)] = dir.fileSize();
      }
      return sendJson(json);
    }
  }
// PATTERNS - list all patterns
  else if (path == "patterns") 
  {  
    if (method == HTTP_GET) {
      Dir dir = SPIFFS.openDir("/p/");
      while (dir.next()) {
        json[dir.fileName()] = dir.fileSize();
      }
      return sendJson(json);
    }
  }
// ACTIONS - perform various actions
  else if (path == String("action"))
  {
    
    if (method == HTTP_POST && server.hasArg("plain")) {
      deserializeJson(json, server.arg("plain"));
      if (json.containsKey("action")) {
        String action = json["action"];
        
        if (action == "play")
        {
          if (json.containsKey("animation")) {
            playAnimation(json["animation"]);
            return sendOK();
          } else if (json.containsKey("pattern")) {
            String pattern = json["pattern"];
            // TODO play pattern
            return sendOK();
          }
        }
        else if (action == "reboot")
        {
          delay(500);
          ESP.restart();
          return sendOK();
        }
        
      }
    }
  }
  else if (path == String("sysinfo"))
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

    return sendJson(json);
  }

  return false;
}


void handleGetLEDs()
{
  server.send(200, "application/octet-stream", (const char*)&leds, NUM_LEDS*3);
}


bool handleSaveAnimation()
{
  if (server.hasArg("animation") && server.hasArg("name")) {
    String name = String(server.arg("name"));
    name.replace(" ", "_");
    
    String filename = "/a/" + name + ".lua";
    File file = SPIFFS.open(filename, "w");
    file.print(String(server.arg("animation")));
    file.close();

    return sendRedirect("/animations/edit?a=" + name);
  }
  return sendError();
}


uint8_t flip = 0;
void sendLEDs()
{
  if (digitalRead(REQ_PIN) == HIGH) {
    return;
  }

  if (flip < 255) flip++;
  else flip=0;
  leds[3] = RGB(flip);

  for (uint32_t b=0; b<NUM_LEDS*3; b++) {
    Serial1.write(((uint8_t*)&leds)[b]);
  }
  
  delay(1);
}

void setup(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  pinMode(REQ_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  Serial1.begin(1000000);
  
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
  server.on("/",                HTTP_GET,  []() { handleFile ("/home.html"); } );
  server.on("/leds",            HTTP_GET,  handleGetLEDs);
  server.on("/animations",      HTTP_GET,  []() { handleFile ("/animations.html"); } );
  server.on("/animations/edit", HTTP_GET,  []() { handleFile ("/animations-edit.html"); } );
  server.on("/animations/edit", HTTP_POST, []() { handleSaveAnimation(); });
  server.on("/patterns",        HTTP_GET,  []() { handleFile ("/patterns.html"); } );
  server.on("/patterns/edit",   HTTP_GET,  []() { handleFile ("/patterns-edit.html"); } );
  
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
    leds[i] = RGB(0xAA,0xAA,0xAA);
  }
  leds[0] = RGB(255,0,0);
  leds[1] = RGB(0,255,0);
  leds[2] = RGB(0,0,255);

  t = millis();

  playAnimation("example");
}


void loop(void)
{
  server.handleClient();
  MDNS.update();

  uint32_t dt = millis() - t;

  //animate(dt);
  
  // TODO prepare next frame here
  sendLEDs();

  t = millis();
}
