
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

String getContentType(String filename)
{
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js"))  return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".png")) return "image/png";
  return "text/plain";
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

double clamp(double value, double min, double max)
{
  if (value > max) return max;
  if (value < min) return min;
  return value;
}
