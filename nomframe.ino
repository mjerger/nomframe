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


const int led = LED_BUILTIN;


void errorLoop() {
  while (true) {
    for (int i=0; i<3; i++) {
      digitalWrite(led, LOW);
      delay(100);
      digitalWrite(led, HIGH);
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


void setup(void)
{
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
  
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
}

void loop(void)
{
  server.handleClient();
  MDNS.update();
}
