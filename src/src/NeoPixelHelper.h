#include <Adafruit_NeoPixel.h>

// Pack color data into 32 bit unsigned int (copied from Neopixel library).
uint32_t Color(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint32_t)((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Input a value 0 to 255 to get a color value (of a pseudo-rainbow).
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
