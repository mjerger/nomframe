// Simple serial to fastLED interface

//#define NO_CLOCK_CORRECTION 1
#include <FastLED.h>

#define REQ_PIN 5
#define LED_PIN 2

#define NUM_LEDS 256
CRGB leds[NUM_LEDS];

void setup()
{
  pinMode(REQ_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64);

  Serial.begin(1000000);

  for (uint16_t i=0; i<NUM_LEDS; i++) leds[i] = CRGB::Black;
  leds[0] = CRGB::Green;
  FastLED.show();
}


void loop()
{
  Serial.write("r");
  digitalWrite(REQ_PIN, LOW);
  
  uint16_t numRead = 0;
  while (numRead < NUM_LEDS*3) {
    numRead += Serial.readBytes((uint8_t*)&leds + numRead, 16*3);
  }
  
  digitalWrite(REQ_PIN, HIGH);
  
  FastLED.show();
}
