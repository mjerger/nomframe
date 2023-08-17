/* 
 *  nomframe
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
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <FastLED.h>
#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"
#include <time.h>
#include "tinyexpr.h"
#include "utils.h"

// secrets
#include "config.h"

const String methods[8]{ "ANY", "GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS" };
ESP8266WebServer server(80);
File uploadFile;

// NTP
#define MY_NTP_SERVER "at.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
time_t now;
tm tm;
const String weekdays[7]{ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// LEDs
#define MAX_BRIGHTNESS 128
#define FPS 30
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS (WIDTH * HEIGHT)
CRGB leds[NUM_LEDS];
#define LED_PIN 4

boolean ledsEnabled = true;

//#define SERIAL_ENABLED


// Animations
struct Playing {
  String animation = "";
  String data = "";
};

Playing playing;
boolean reloadExpr = false;
boolean exprError = false;
CRGBPalette16 palette = RainbowColors_p;

////////////////////////////////////////////////////////////////////////////////

void playAnimation(String animation) 
{
  String path = (animation.startsWith("/ani/") ? animation : "/ani/" + animation) + ".json";

  if (SPIFFS.exists(path) == false) {
    return;
  }

  // Read animation data
  File file = SPIFFS.open(path, "r");
  String data = "";
  while (file.available()) {
    data += file.readStringUntil('\n');
  }

  // Parse JSON doc
  DynamicJsonDocument doc(256);
  deserializeJson(doc, data);

  // Start animation
  playing.data = String(doc["f"]);
  playing.animation = animation;
  reloadExpr = true;

  Serial.println("playing " + animation);
}

// Map -1 .. +1 to an RGB value via a color palette
CRGB rgbMap(double value) {
  // Wrapped map by modulo
  int intval = (int)value;
  double v = map(value - (double)(intval), -1, 1, 0, 255);

  // Palette map
  return ColorFromPalette(palette, v, 255, LINEARBLEND);
}

// Linear map between two ranges
double map(double in, double in_from, double in_to, double out_from, double out_to) 
{
  double out = (in - in_from) / (in_to - in_from);
  out = out * (out_to - out_from) + out_from;
  return out;
}

// Zig-Zag LED panel pattern
uint16_t ledMap(int x, int y) 
{
  if (y % 2) x = WIDTH - 1 - x;
  return y * WIDTH + x;
}


// Response with file from FS
bool handleFile(String path) 
{
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  // Send requested (compressed) file
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {

    if (SPIFFS.exists(pathWithGz)) path += ".gz";

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

  DynamicJsonDocument json(200);

  // FILES - list all files
  if (path == "files") {
    if (method == HTTP_GET) {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        json[dir.fileName()] = dir.fileSize();
      }
      return sendJson(json);
    }
  }
  // ANIMATIONS - list all animations
  else if (path == "animations") {
    if (method == HTTP_GET) {
      Dir dir = SPIFFS.openDir("/ani/");
      while (dir.next()) {
        String filename = dir.fileName().substring(5);
        if (filename.endsWith(".json")) {
          filename = filename.substring(0, filename.length() - 5);
          json[filename] = dir.fileSize();
        }
      }
      return sendJson(json);
    }
  }
  // ACTIONS - perform various actions
  else if (path == "action") {

    if (method == HTTP_POST && server.hasArg("plain")) {
      deserializeJson(json, server.arg("plain"));
      if (json.containsKey("action")) {
        String action = json["action"];

        if (action == "play") {
          if (json.containsKey("animation")) {
            playAnimation(json["animation"]);
            return sendOK();
          }
        } else if (action == "reboot") {
          delay(500);
          ESP.restart();
          return sendOK();
        }
      }
    }
  // SYSINFO
  } else if (path == "sysinfo") {
    // Uptime
    json["uptime"] = uptime_formatter::getUptime();

    // NTP time
    time(&now);
    localtime_r(&now, &tm);
    json["datetime"] = weekdays[tm.tm_wday] + " " + leadingZero(tm.tm_mday) + "." + leadingZero(tm.tm_mon + 1) + "." + String(tm.tm_year + 1900) + " " + leadingZero(tm.tm_hour + 1) + ":" +  // No idea why it's off by one.. maybe DST is not working??
                       leadingZero(tm.tm_min) + ":" + leadingZero(tm.tm_sec);

    // Network
    json["ssid"] = String(ssid);
    json["host"] = String(mdnsHostname) + ".local";
    json["ip"] = WiFi.localIP();

    // Filesystem
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    json["fs_free"] = String(double(fs_info.totalBytes - fs_info.usedBytes) / 1024.0, 1) + "k";
    json["fs_used"] = String((double)fs_info.usedBytes / 1024.0, 1) + "k";
    json["fs_total"] = String((double)fs_info.totalBytes / 1024.0, 1) + "k";

    return sendJson(json);
  } else if (path == String("leds")) {
    json["leds"] = NUM_LEDS;
    json["width"] = WIDTH;
    json["height"] = WIDTH;

    return sendJson(json);

  // Turn ON LEDs
  } else if (path == "on") {
    ledsEnabled = true;
    return sendOK();
  
  // Turn OFF LEDs
  } else if (path == "off") {
    ledsEnabled = false;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    return sendOK();

  // FLIP LED status
  } else if (path == "flip") {
    if (ledsEnabled) {
      ledsEnabled = false;
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
    } else {
      ledsEnabled = true;
    }
    return sendOK();
  
  // Set outpout brightness
  } else if (path == "brightness") {
    int val = server.arg("val").toInt();
    setBrightness(val);
    if (val == 0) {
      ledsEnabled = false;
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
    } else {
      ledsEnabled = true;
    }
    return sendOK();
  }

  return false;
}


void handleGetLEDs()
{
  server.send(200, "application/octet-stream", (const char *)&leds, NUM_LEDS * 3);
}


bool handleSaveAnimation()
{
  if (server.hasArg("animation") && server.hasArg("name")) {
    String name = String(server.arg("name"));
    name.replace(" ", "_");

    String filename = "/ani/" + name + ".json";
    File file = SPIFFS.open(filename, "w");
    String data = server.arg("animation");
    file.write(data.c_str(), data.length());
    file.close();

    return sendRedirect("/animations/edit?a=" + name);
  }
  return sendError();
}


bool sendJson(DynamicJsonDocument json)
{
  String string;
  serializeJson(json, string);
  server.send(200, "application/json", string);
  return true;
}


bool sendOK()
{
  server.send(200);
  return true;
}


bool sendRedirect(String location)
{
  server.sendHeader("Location", location);
  server.send(303);
  return true;
}


bool sendError()
{
  server.send(500, "text/plain", "500: Internal Server Error");
  return true;
}


void setBrightness(int val)
{
  val = min(MAX_BRIGHTNESS, max(0, val));
  FastLED.setBrightness((double)(val)/ 100.0 * (double)MAX_BRIGHTNESS);
}


void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  #ifdef SERIAL_ENABLED
    Serial.begin(115200);
  #endif
  
  delay(100);
  Serial.println("\n\n--------------");
  Serial.println(" nomframe 1.0");
  Serial.println("--------------");

  Serial.print("Initializing SPIFFS ... ");
  if (SPIFFS.begin()) {
    Serial.print("ok");
  } else {
    Serial.println("failed");
    errorLoop();
  }

  // LEDs
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  setBrightness(75);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,5000);

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
  server.on("/", HTTP_GET, []() {
    handleFile("/home.html");
  });
  server.on("/leds", HTTP_GET, handleGetLEDs);
  server.on("/animations", HTTP_GET, []() {
    handleFile("/animations.html");
  });
  server.on("/animations/edit", HTTP_GET, []() {
    handleFile("/animations-edit.html");
  });
  server.on("/animations/edit", HTTP_POST, handleSaveAnimation);
  server.on("/settings", HTTP_GET, []() {
    handleFile("/settings.html");
  });

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

  // Start Server
  server.begin();
  Serial.println("HTTP server started");

  playAnimation("example");
}


void loop(void) {

  // Handle server / network
  server.handleClient();
  MDNS.update();

  if (ledsEnabled) {
  
    // Process animation
    static double x, y, t, v;
    static te_variable vars[] = { { "x", &x }, { "y", &y }, { "t", &t } };
    static int err;
    static te_expr *expr = 0;
    if (reloadExpr) {
      te_free(expr);
      expr = 0;
      reloadExpr = false;
    }
    if (!expr) expr = te_compile(playing.data.c_str(), vars, 3, &err);
    if (expr) {
      t = millis() / 1000.0;
      for (int x_i = 0; x_i < 16; x_i++) {
        for (int y_i = 0; y_i < 16; y_i++) {
          x = ((double)x_i - 7.5) / 7.5; // -1 .. +1
          y = ((double)y_i - 7.5) / 7.5; // -1 .. +1
          v = te_eval(expr);             // -1 .. +1
          leds[ledMap(x_i, y_i)] = rgbMap(v);
        }
      }
    }
  
    exprError = !expr;
    
  }
  
  // Output
  FastLED.delay(1000/FPS);  
}
