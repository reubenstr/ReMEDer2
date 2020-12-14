//
// ReMEDer
// Version: 2
//
// A flashing alarm/indicator as a reminder to take medicine before bedtime.
//
// Reuben Strangelove
// Winter 2020
//
// MCU: atmega328p (Arduino Mini)
// LCD: SSD1306 OLED 128 x 32 
// LEDs: Neopixel strip 

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RtcDS1307.h> // RTC Library: https://github.com/Makuna/Rtc
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <JC_Button.h> // https://github.com/JChristensen/JC_Button

#define PIN_BUTTON_NEXT 2
#define PIN_BUTTON_PREV 3
#define PIN_BUTTON_SELECT 4
#define PIN_BUTTON_RESET 7
#define PIN_LED_RESET_BUTTON 8
#define PIN_LED_STRIP 9
#define PIN_LED_BUILTIN 13

#define DELAY_DEBOUNCE_MS 50
#define DELAY_INDICATOR_MS 10 // Rise/Decay time for indicator (ms)

const int selectedItemFlash = 500;

RtcDS1307<TwoWire> Rtc(Wire);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int numSpacesLCD = 9;

Button buttonReset(PIN_BUTTON_RESET);
Button buttonSelect(PIN_BUTTON_SELECT);
Button buttonPrev(PIN_BUTTON_PREV);
Button buttonNext(PIN_BUTTON_NEXT);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

const unsigned long
    REPEAT_FIRST(500), // ms required before repeating on long press
    REPEAT_INCR(100);  // repeat interval for long press

#define countof(a) (sizeof(a) / sizeof(a[0]))

int timeHour, timeMinute, alarmHour, alarmMinute;
int oldTimeHour, oldTimeMinute, oldAlarmHour, oldAlarmMinute;
bool indicatorOn = false;

bool newRandomColorFlag;

enum Menu
{
  TIME_HOUR,
  TIME_MIN,
  NUMALARMS,
  ALARM_HOUR,
  ALARM_MIN,
  COLOR,
  PATTERN,
  SPEED,
  MAX_MENUITEM
};
int selectedMenuItem = TIME_HOUR;

int numAlarms = 1;
const int maxNumAlarms = 9;
int selectedAlarm = 1;
struct Alarm
{
  int hour;
  int minute;
};
Alarm alarms[maxNumAlarms + 1];

const char *colorText[5] = {"Red", "Green", "Blue", "Random", "Rainbow"};
enum Colors
{
  RED,
  GREEN,
  BLUE,
  RANDOM,
  RAINBOW,
  MAX_COLOR
};
int selectedColor = RED;

const char *patternText[5] = {"Flash", "Sinwave", "Strobe", "Sparkle", "Chase"};
enum Patterns
{
  FLASH,
  SINWAVE,
  STROBE,
  SPARKLE,
  CHASE,
  MAX_PATTERN
};
int selectedPattern = FLASH;

const char *speedText[5] = {"Slow", "Medium", "Fast"};
enum Speeds
{
  SLOW,
  MEDIUM,
  FAST,
  MAX_SPEED
};
int selectedSpeed = FLASH;

void Error(int x)
{
  // Loop forever, indicates fatal error.
  analogWrite(PIN_LED_RESET_BUTTON, 0);
  delay(1000);
  analogWrite(PIN_LED_RESET_BUTTON, 127);
  delay(x);
}

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

void SetFullStripToColor()
{
  uint32_t color;

  if (selectedColor == RED)
  {
    strip.fill(strip.Color(255, 0, 0), 0, strip.numPixels());
  }
  else if (selectedColor == GREEN)
  {
    strip.fill(strip.Color(0, 255, 0), 0, strip.numPixels());
  }
  else if (selectedColor == BLUE)
  {
    strip.fill(strip.Color(0, 0, 255), 0, strip.numPixels());
  }
  else if (selectedColor == RANDOM)
  {
    static uint32_t color;
    static byte wheelPos = 0;
    if (newRandomColorFlag)
    {
      newRandomColorFlag = false;
      wheelPos = random(0, 256);
    }
    strip.fill(Wheel(wheelPos), 0, strip.numPixels());
  }
  else if (selectedColor == RAINBOW)
  {
    static byte wheelPos = 0;
    static unsigned long last = millis();
    if (millis() - last > 50)
    {
      last = millis();
      wheelPos++;
    }

    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, Wheel(wheelPos + i * (255 / strip.numPixels())));
    }
  }
}

void ProcessIndicator(bool isOn)
{
  static unsigned int pwmValue = 0;
  static unsigned long pwmMillis;

  SetFullStripToColor();

  if (selectedPattern == FLASH)
  {
    static bool toggleFlag = true;
    static unsigned long last = millis();
    if (millis() - last > 3000)
    {
      last = millis();
      toggleFlag = !toggleFlag;
      newRandomColorFlag = true;
    }
    strip.setBrightness(toggleFlag ? 255 : 0);
  }
  else if (selectedPattern == SINWAVE)
  {
    static unsigned int sinValue = 0;
    static unsigned long last = millis();
    if (millis() - last > 1)
    {
      last = millis();
      sinValue++;
      if (sinValue == 0)
      {
        newRandomColorFlag = true;
      }
    }
    strip.setBrightness((255 / 2) + (255 / 2) * sin(radians(sinValue)));
  }
  else if (selectedPattern == STROBE)
  {
    static unsigned long last = millis();
    static bool toggleFlag = true;

    int delay = toggleFlag ? 1000 / 10 : 1000;

    if (millis() - last > delay)
    {
      last = millis();
      newRandomColorFlag = true;
      toggleFlag = !toggleFlag;
    }
    strip.setBrightness(toggleFlag ? 255 : 0);
  }
  else if (selectedPattern == SPARKLE)
  {
    static byte index = 0;
    static unsigned long last = millis();
    if (millis() - last > 100)
    {
      last = millis();
      newRandomColorFlag = true;
      index = random(0, strip.numPixels());
    }
    strip.setBrightness(255);
    for (int i = 0; i < strip.numPixels(); i++)
    {
      if (i != index)
        strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  else if (selectedPattern == CHASE)
  {
    static byte index = 0;
    static unsigned long last = millis();
    if (millis() - last > 100)
    {
      last = millis();
      newRandomColorFlag = true;
      index++;
      if (index >= strip.numPixels())
      {
        index = 0;
      }
    }
    strip.setBrightness(255);
    for (int i = 0; i < strip.numPixels(); i++)
    {
      if (i != index)
        strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }

  strip.show();
}

bool ProcessResetButton()
{
  buttonReset.read();
  return buttonReset.wasPressed();
}

bool ProcessControlButtons()
{

  bool updatePerformedFlag = false;

  buttonSelect.read();
  buttonPrev.read();
  buttonNext.read();

  // Button: Select
  if (buttonSelect.wasPressed())
  {
    updatePerformedFlag = true;

    // Treat alarms as pseudo submenues and cycle through them.
    if (selectedMenuItem == NUMALARMS)
    {
      selectedAlarm = 1;
    }

    if (selectedMenuItem == ALARM_MIN)
    {
      selectedMenuItem = ALARM_HOUR;
      selectedAlarm++;
      if (selectedAlarm > numAlarms)
      {
        selectedAlarm = 0;
        selectedMenuItem += 2;
      }
    }
    else
    {
      selectedMenuItem++;
    }

    if (selectedMenuItem >= MAX_MENUITEM)
    {
      selectedMenuItem = 0;
    }
  }

  // Button: Prev
  if (buttonPrev.wasPressed())
  {
    updatePerformedFlag = true;

    if (selectedMenuItem == TIME_HOUR)
    {
      if (timeHour == 0)
      {
        timeHour = 23;
      }
      else
      {
        timeHour--;
      }
    }
    else if (selectedMenuItem == TIME_MIN)
    {
      if (timeMinute == 0)
      {
        timeMinute = 59;
      }
      else
      {
        timeMinute--;
      }
    }
    else if (selectedMenuItem == NUMALARMS)
    {
      if (numAlarms == 1)
      {
        numAlarms = maxNumAlarms;
      }
      else
      {
        numAlarms--;
      }
    }
    else if (selectedMenuItem == ALARM_HOUR)
    {
      if (alarms[selectedAlarm].hour = 0)
      {
        alarms[selectedAlarm].hour = 23;
      }
      else
      {
        alarms[selectedAlarm].hour--;
      }
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      if (alarms[selectedAlarm].minute = 0)
      {
        alarms[selectedAlarm].minute = 59;
      }
      else
      {
        alarms[selectedAlarm].minute--;
      }
    }
    else if (selectedMenuItem == COLOR)
    {
      if (selectedColor == 0)
      {
        selectedColor = MAX_COLOR - 1;
      }
      else
      {
        selectedColor--;
      }
    }
    else if (selectedMenuItem == PATTERN)
    {
      if (selectedPattern == 0)
      {
        selectedPattern = MAX_PATTERN - 1;
      }
      else
      {
        selectedPattern--;
      }
    }
    else if (selectedMenuItem == SPEED)
    {
      if (selectedSpeed == 0)
      {
        selectedSpeed = MAX_SPEED - 1;
      }
      else
      {
        selectedSpeed--;
      }
    }
  }

  // Button: Next
  if (buttonNext.wasPressed())
  {
    updatePerformedFlag = true;

    if (selectedMenuItem == TIME_HOUR)
    {
      timeHour++;
      if (timeHour > 23)
      {
        timeHour = 0;
      }
    }
    else if (selectedMenuItem == TIME_MIN)
    {
      timeMinute++;
      if (timeMinute > 59)
      {
        timeMinute = 0;
      }
    }
    else if (selectedMenuItem == NUMALARMS)
    {
      numAlarms++;
      if (numAlarms >= maxNumAlarms)
      {
        numAlarms = 1;
      }
    }
    else if (selectedMenuItem == ALARM_HOUR)
    {
      alarms[selectedAlarm].hour++;
      if (alarms[selectedAlarm].hour > 59)
      {
        alarms[selectedAlarm].hour = 0;
      }
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      alarms[selectedAlarm].minute++;
      if (alarms[selectedAlarm].minute > 59)
      {
        alarms[selectedAlarm].minute = 0;
      }
    }
    else if (selectedMenuItem == COLOR)
    {
      selectedColor++;
      if (selectedColor >= MAX_PATTERN)
      {
        selectedColor = 0;
      }
    }
    else if (selectedMenuItem == PATTERN)
    {
      selectedPattern++;
      if (selectedPattern >= MAX_PATTERN)
      {
        selectedPattern = 0;
      }
    }
    else if (selectedMenuItem == SPEED)
    {
      selectedSpeed++;
      if (selectedSpeed >= MAX_SPEED)
      {
        selectedSpeed = 0;
      }
    }
  }

  return updatePerformedFlag;
}

void UpdateDisplay(bool activeFlag = true)
{

  static unsigned long flashMillis;
  static bool displayValue = true;

  char buf[20];

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

  int flashDelay = displayValue ? 600 : 100; // selectedItemFlash

  if ((flashMillis + flashDelay) < millis())
  {
    flashMillis = millis();
    displayValue = !displayValue;
  }

  // Update entire display when new menu item is selected.
  // Display first row.
  static bool selectedMenuItemBuffer = 99; // Force refresh upon startup.
  if (selectedMenuItemBuffer != selectedMenuItem)
  {
    selectedMenuItemBuffer = selectedMenuItem;
    display.clearDisplay();
    display.setCursor(0, 0);

    if (selectedMenuItem == TIME_HOUR || selectedMenuItem == TIME_MIN)
    {
      display.println("Time");
    }
    else if (selectedMenuItem == NUMALARMS)
    {
      display.println("No. Alarms");
    }
    else if (selectedMenuItem == ALARM_HOUR || selectedMenuItem == ALARM_MIN)
    {
      sprintf(buf, "Alarm: %u", selectedAlarm);
      display.println(buf);
    }
    else if (selectedMenuItem == COLOR)
    {
      display.println("Color");
    }
    else if (selectedMenuItem == PATTERN)
    {
      display.println("Pattern");
    }
    else if (selectedMenuItem == SPEED)
    {
      display.println("Speed");
    }
  }

  // Display second row.
  display.setCursor(0, 16);

  if (selectedMenuItem == TIME_HOUR)
  {
    if (displayValue)
      sprintf(buf, "%02u:%02u", timeHour, timeMinute);
    else
      sprintf(buf, "  :%02u", timeMinute);
  }
  if (selectedMenuItem == TIME_MIN)
  {
    if (displayValue)
      sprintf(buf, "%02u:%02u", timeHour, timeMinute);
    else
      sprintf(buf, "%02u:  ", timeHour);
  }
  else if (selectedMenuItem == NUMALARMS)
  {
    display.println(numAlarms);
  }
  else if (selectedMenuItem == ALARM_HOUR)
  {
    if (displayValue)
      sprintf(buf, "%02u:%02u", alarms[selectedAlarm].hour, alarms[selectedAlarm].minute);
    else
      sprintf(buf, "  :%02u", alarms[selectedAlarm].minute);
  }
  else if (selectedMenuItem == ALARM_MIN)
  {
    if (displayValue)
      sprintf(buf, "%02u:%02u", alarms[selectedAlarm].hour, alarms[selectedAlarm].minute);
    else
      sprintf(buf, "%02u:  ", alarms[selectedAlarm].hour);
  }
  else if (selectedMenuItem == COLOR)
  {
    if (displayValue)
    {
      sprintf(buf, "%-9s\n", colorText[selectedColor]);
      display.println(buf);
    }
    else
      display.println("         ");
  }
  else if (selectedMenuItem == PATTERN)
  {
    if (displayValue)
    {
      sprintf(buf, "%-9s\n", patternText[selectedPattern]);
      display.println(buf);
    }
    else
      display.println("         ");
  }
  else if (selectedMenuItem == SPEED)
  {
    if (displayValue)
    {
      sprintf(buf, "%-9s\n", speedText[selectedSpeed]);
      display.println(buf);
    }
    else
      display.println("         ");
  }

  display.println(buf);
  display.display();
}

// Debug.
void printDateTime(const RtcDateTime &dt)
{
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  Serial.print(datestring);
}

void SetupRTC()
{
  // Setup code provided by library example:
  // https://github.com/Makuna/Rtc/blob/master/examples/DS1307_Simple/DS1307_Simple.ino

  strip.begin();
  strip.show();

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid())
  {
    if (Rtc.LastError() != 0)
    {
      // we have a communications error
      // see https://www.arduino.cc/en/Reference/WireEndTransmission for
      // what the number means
      Serial.print("RTC communications error = ");
      Serial.println(Rtc.LastError());
      Error(100);
    }
    else
    {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      Serial.println("RTC lost confidence in the DateTime!");
      // following line sets the RTC to the date & time this sketch was compiled
      // it will also reset the valid flag internally unless the Rtc device is
      // having an issue

      Rtc.SetDateTime(compiled);
    }
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled)
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled)
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low);

  Serial.println("RTC setup finished.");
}

void LoadEEPROMData()
{
  selectedColor = EEPROM.read(0);
  selectedPattern = EEPROM.read(1);
  selectedSpeed = EEPROM.read(2);
  numAlarms = EEPROM.read(3);

  for (int i = 0; i < maxNumAlarms; i++)
  {
    alarms[i].hour = EEPROM.read(16 + i);
    alarms[i].minute = EEPROM.read(32 + i);

    if (alarms[i].hour = 255)
    {
      alarms[i].hour = 0;
    }
    if (alarms[i].minute = 255)
    {
      alarms[i].minute = 0;
    }
  }

  if (selectedColor == 255)
  {
    selectedColor = 0;
  }
  if (selectedPattern == 255)
  {
    selectedPattern = 0;
  }
  if (selectedSpeed == 255)
  {
    selectedSpeed = 0;
  }
  if (numAlarms == 255)
  {
    numAlarms = 0;
  }
}

void SaveEEPROMData()
{
  static int oldTimeHour, oldTimeMinute;
  static Alarm oldAlarms[maxNumAlarms];
  static int oldSelectedColor, oldSelectedPattern, oldSelectedSpeed;
  static int oldNumAlarms;

  // Check if variables to be saved to eeprom or RTC have changed.
  static unsigned long eepromMillis;
  if ((eepromMillis + 5000) < millis())
  {
    eepromMillis = millis();

    // Check if time was updated by the user.
    if (oldTimeHour != timeHour || oldTimeMinute != timeMinute)
    {
      Rtc.SetDateTime(RtcDateTime(2020, 1, 1, timeHour, timeMinute, 0));
    }

    // Check if an alarm was updated by the user.
    for (int i = 0; i < maxNumAlarms; i++)
    {
      if (oldAlarms[i].hour != alarms[i].hour || oldAlarms[i].minute != alarms[i].minute)
      {
        oldAlarms[i].hour = alarms[i].hour;
        oldAlarms[i].minute = alarms[i].minute;
        EEPROM.write(16 + i, alarms[i].hour);
        EEPROM.write(32 + i, alarms[i].minute);
      }
    }

    // Check if options was updated by the user.
    if (oldSelectedColor != selectedColor)
    {
      oldSelectedColor = selectedColor;
      EEPROM.write(0, selectedColor);
    }

    if (oldSelectedPattern != selectedPattern)
    {
      oldSelectedPattern = selectedPattern;
      EEPROM.write(1, selectedPattern);
    }

    if (oldSelectedSpeed != selectedSpeed)
    {
      oldSelectedSpeed = selectedSpeed;
      EEPROM.write(2, selectedSpeed);
    }
    if (oldNumAlarms != numAlarms)
    {
      oldNumAlarms = numAlarms;
      EEPROM.write(3, numAlarms);
    }
  }
}

void BlinkOnboardLED()
{
  static unsigned long builtinLedMillis;
  if ((builtinLedMillis + 500) < millis())
  {
    builtinLedMillis = millis();
    digitalWrite(PIN_LED_BUILTIN, !digitalRead(PIN_LED_BUILTIN));
  }
}

void setup()
{
  Serial.begin(115200);

  Serial.println("ReMEDer starting up...");

  delay(1000);

  pinMode(PIN_LED_BUILTIN, OUTPUT);
  pinMode(PIN_LED_RESET_BUTTON, OUTPUT);

  buttonReset.begin();
  buttonSelect.begin();
  buttonPrev.begin();
  buttonNext.begin();

  SetupRTC();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    Error(1000);
  }

  LoadEEPROMData();

  //RtcDateTime dateTime = Rtc.GetDateTime();
  //timeHour = dateTime.Hour();
  //timeMinute = dateTime.Minute();
  //UpdateDisplay(timeHour, timeMinute, alarmHour, alarmMinute);
}

void loop()
{
  BlinkOnboardLED();

  bool userUpdateFlag = ProcessControlButtons();



  UpdateDisplay();

  // Check if time has updated.
  if (Rtc.IsDateTimeValid())
  {
    RtcDateTime dateTime = Rtc.GetDateTime();
    timeHour = dateTime.Hour();
    timeMinute = dateTime.Minute();

    // Time updated from RTC
    if (oldTimeHour != timeHour || oldTimeMinute != timeMinute)
    {
      oldTimeHour = timeHour;
      oldTimeMinute = timeMinute;

      // Check for alarm trigger
      if (timeHour == alarmHour && timeMinute == alarmMinute)
      {
        indicatorOn = true;
      }
    }
  }
  else
  {
    // RTC error, likely bad battery.
    Error(100);
  }

  if (ProcessResetButton())
  {
    indicatorOn = false;
  }

  ProcessIndicator(indicatorOn);

  SaveEEPROMData();
}