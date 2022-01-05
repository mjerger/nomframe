// Simple serial bitbang to fastLED interface

//#define NO_CLOCK_CORRECTION 1
#include <FastLED.h>

#define REQ_PIN 0
#define CLK_PIN 1
#define DAT_PIN 2
#define LED_PIN 3

#define NUM_LEDS 120
CRGB leds[NUM_LEDS];

volatile unsigned long timer0_millis = 0;

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(REQ_PIN, OUTPUT);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DAT_PIN, INPUT_PULLUP);
  
  digitalWrite(REQ_PIN, HIGH);

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64);

  for (int i=0; i<NUM_LEDS; i++) leds[i] = CRGB::Black;
  leds[0] = CRGB::White;
  FastLED.show();
}


void loop()
{
  // Request data
  digitalWrite(REQ_PIN, LOW);
  
  static uint8_t clk = 0;
  static uint32_t color = 0;

  // Read all pixel colors
  for (uint8_t l=0; l<NUM_LEDS; l++) {
    
    // Read RGB value
    color = 0;
    for (uint8_t i=0; i<24; i++) {
    
      // Wait for clock change 
      while (digitalRead(CLK_PIN) == clk);
      clk = 1-clk;

      uint8_t data = digitalRead(DAT_PIN) == HIGH ? 1 : 0;
      color = (color << 1) | data;
    }

    // Set LED color
    leds[l] = CRGB(color);
  }

  // Done reading
  digitalWrite(REQ_PIN, HIGH);

  // Output to LED strip
  FastLED.show();
}
