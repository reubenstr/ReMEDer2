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
//
// Known issues:
//    LCD has corruption on lower right of screen. EMI could be the cause due to a
//      noisy DC-DC power supply.
//    MCU crashes upon serial prints during EEPROM read or load. Likely a low RAM
//      issue.

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
#include "NeoPixelHelper.h" // Local

#define PIN_BUTTON_NEXT 2
#define PIN_BUTTON_PREV 3
#define PIN_BUTTON_SELECT 4
#define PIN_BUTTON_RESET 7
#define PIN_LED_RESET_BUTTON 5
#define PIN_LED_STRIP 9
#define PIN_LED_BUILTIN 13

const int selectedItemFlash = 500;

RtcDS1307<TwoWire> Rtc(Wire);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int numSpacesLCD = 9;
bool displayOnFlag = true;

Button buttonReset(PIN_BUTTON_RESET);
Button buttonSelect(PIN_BUTTON_SELECT);
Button buttonPrev(PIN_BUTTON_PREV);
Button buttonNext(PIN_BUTTON_NEXT);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

#define countof(a) (sizeof(a) / sizeof(a[0]))

int timeHour, timeMinute, alarmHour, alarmMinute;

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
int selectedAlarm;
struct Alarm
{
  byte hour;
  byte minute;
};
Alarm alarms[maxNumAlarms];
Alarm oldAlarms[maxNumAlarms];

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

void Error()
{
  // Loop forever, indicates fatal error.
  while (1)
  {
    analogWrite(PIN_LED_RESET_BUTTON, 0);
    delay(500);
    analogWrite(PIN_LED_RESET_BUTTON, 127);
    delay(100);
  }
}

void SetFullStripToColor()
{
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

    for (unsigned int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, Wheel(wheelPos + i * (255 / strip.numPixels())));
    }
  }
}

void ProcessIndicator(bool indicatorOn)
{
  if (!indicatorOn)
  {
    strip.fill(strip.Color(0, 0, 0), 0, strip.numPixels());
    strip.show();
    return;
  }

  SetFullStripToColor();

  if (selectedPattern == FLASH)
  {
    static bool toggleFlag = true;
    static unsigned long last = millis();

    unsigned int delay = selectedSpeed == 0 ? 2000 : selectedSpeed == 1 ? 1000 : selectedSpeed == 2 ? 500 : 100;
    if (millis() - last > delay)
    {
      last = millis();
      toggleFlag = !toggleFlag;
      newRandomColorFlag = true;
    }
    strip.setBrightness(toggleFlag ? 255 : 0);
  }
  else if (selectedPattern == SINWAVE)
  {
    static int sinValue = 0;
    static unsigned long last = millis();

    unsigned int delay = selectedSpeed == 0 ? 10 : selectedSpeed == 1 ? 5 : selectedSpeed == 2 ? 1 : 0;
    if (millis() - last > delay)
    {
      last = millis();

      sinValue++;
      if (sinValue == 361)
      {
        sinValue = 0;
      }
      if (sinValue == 180)
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

    unsigned int delay = selectedSpeed == 0 ? 3000 : selectedSpeed == 1 ? 1500 : selectedSpeed == 2 ? 500 : 100;
    unsigned int strobeDelay = toggleFlag ? 250 : delay;
    if (millis() - last > strobeDelay)
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

    unsigned int delay = selectedSpeed == 0 ? 1000 : selectedSpeed == 1 ? 500 : selectedSpeed == 2 ? 100 : 100;
    if (millis() - last > delay)
    {
      last = millis();
      newRandomColorFlag = true;
      index = random(0, strip.numPixels());
    }
    strip.setBrightness(255);
    for (unsigned int i = 0; i < strip.numPixels(); i++)
    {
      if (i != index)
        strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  else if (selectedPattern == CHASE)
  {
    static byte index = 0;
    static unsigned long last = millis();

    unsigned int delay = selectedSpeed == 0 ? 500 : selectedSpeed == 1 ? 250 : selectedSpeed == 2 ? 100 : 100;
    if (millis() - last > delay)
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
    for (unsigned int i = 0; i < strip.numPixels(); i++)
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

    if (displayOnFlag == false)
    {
      return true;
    }

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

    if (displayOnFlag == false)
    {
      return true;
    }

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
      if (alarms[selectedAlarm - 1].hour == 0)
      {
        alarms[selectedAlarm - 1].hour = 23;
      }
      else
      {
        alarms[selectedAlarm - 1].hour--;
      }
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      if (alarms[selectedAlarm - 1].minute == 0)
      {
        alarms[selectedAlarm - 1].minute = 59;
      }
      else
      {
        alarms[selectedAlarm - 1].minute--;
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

    if (displayOnFlag == false)
    {
      return true;
    }

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
      alarms[selectedAlarm - 1].hour++;
      if (alarms[selectedAlarm - 1].hour > 59)
      {
        alarms[selectedAlarm - 1].hour = 0;
      }
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      alarms[selectedAlarm - 1].minute++;
      if (alarms[selectedAlarm - 1].minute > 59)
      {
        alarms[selectedAlarm - 1].minute = 0;
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

void UpdateDisplay(bool updateFlag)
{
  static unsigned long flashMillis;
  static bool displayValue = true;
  char buf[20];

  // Prevent awkard flashes when user activates a button.
  if (updateFlag)
  {
    displayValue = true;
    flashMillis = millis();
  }

  int flashDelay = displayValue ? 600 : 100; // selectedItemFlash
  if ((flashMillis + flashDelay) < millis())
  {
    flashMillis = millis();
    displayValue = !displayValue;
    // Only flash on certain menu items.
    if (selectedMenuItem == TIME_HOUR || selectedMenuItem == TIME_MIN || selectedMenuItem == ALARM_HOUR || selectedMenuItem == ALARM_MIN)
    {
      updateFlag = true;
    }
  }

  // Only update when there is new data or the data is to be flashed.
  if (updateFlag)
  {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

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
        sprintf(buf, "%02u:%02u", alarms[selectedAlarm - 1].hour, alarms[selectedAlarm - 1].minute);
      else
        sprintf(buf, "  :%02u", alarms[selectedAlarm - 1].minute);
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      if (displayValue)
        sprintf(buf, "%02u:%02u", alarms[selectedAlarm - 1].hour, alarms[selectedAlarm - 1].minute);
      else
        sprintf(buf, "%02u:  ", alarms[selectedAlarm - 1].hour);
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
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid())
  {
    if (Rtc.LastError() != 0)
    {
      Serial.print("RTC communications error = ");
      Serial.println(Rtc.LastError());
      Error();
    }
    else
    {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing
      Serial.println("RTC lost confidence in the DateTime!");
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
    oldAlarms[i].hour = alarms[i].hour;
    oldAlarms[i].minute = alarms[i].minute;

    if (alarms[i].hour == 255)
    {
      alarms[i].hour = 0;
    }
    if (alarms[i].minute == 255)
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
    numAlarms = 1;
  }

  /*
  char buf[50];

  Serial.println("Saved variables from EEPROM...");

  sprintf(buf, "Time -> %02u:%02u", timeHour, timeMinute);
  Serial.println(buf);

  for (int i = 0; i < maxNumAlarms; i++)
  {
    sprintf(buf, "Alarm %u -> %02u:%02u", i + 1, alarms[i].hour, alarms[i].minute);
    Serial.println(buf);
  }

  sprintf(buf, "Color -> %u", selectedColor);
  Serial.println(buf);
  sprintf(buf, "Pattern -> %u", selectedPattern);
  Serial.println(buf);
  sprintf(buf, "Speed -> %u", selectedSpeed);
  Serial.println(buf);
  sprintf(buf, "No. Alarms -> %u", numAlarms);
    Serial.println(buf);    
  */
}

void SaveEEPROMData()
{
  static int oldSelectedColor, oldSelectedPattern, oldSelectedSpeed;
  static int oldNumAlarms;
  char buf[50];

  // Check if variables to be saved to eeprom or RTC have changed.
  static unsigned long eepromMillis;
  if ((eepromMillis + 5000) < millis())
  {
    eepromMillis = millis();

    // Check if an alarm was updated by the user.
    for (int i = 0; i < maxNumAlarms; i++)
    {
      if (oldAlarms[i].hour != alarms[i].hour || oldAlarms[i].minute != alarms[i].minute)
      {
        oldAlarms[i].hour = alarms[i].hour;
        oldAlarms[i].minute = alarms[i].minute;
        EEPROM.write(16 + i, alarms[i].hour);
        EEPROM.write(32 + i, alarms[i].minute);
        // sprintf(buf, "Alarm %u saved as %u:%u.", i, alarms[i].hour, alarms[i].minute);
        // Serial.println(buf);
      }
    }

    // Check if options was updated by the user.
    if (oldSelectedColor != selectedColor)
    {
      oldSelectedColor = selectedColor;
      EEPROM.write(0, selectedColor);
      // sprintf(buf, "Saving color value: %u.", selectedColor);
      // Serial.println(buf);
    }

    if (oldSelectedPattern != selectedPattern)
    {
      oldSelectedPattern = selectedPattern;
      EEPROM.write(1, selectedPattern);
      // sprintf(buf, "Saving pattern value: %u.", selectedPattern);
      // Serial.println(buf);
    }

    if (oldSelectedSpeed != selectedSpeed)
    {
      oldSelectedSpeed = selectedSpeed;
      EEPROM.write(2, selectedSpeed);
      // sprintf(buf, "Saving speed value: %u.", selectedSpeed);
      // Serial.println(buf);
    }
    if (oldNumAlarms != numAlarms)
    {
      oldNumAlarms = numAlarms;
      EEPROM.write(3, numAlarms);
      // sprintf(buf, "Saving numAlarms value: %u.", numAlarms);
      // Serial.println(buf);
    }
  }
}

void BlinkOnboardLED()
{
  static unsigned long builtinLedMillis;
  static bool toggle;
  int delay = toggle ? 100 : 900;
  if ((builtinLedMillis + delay) < millis())
  {
    builtinLedMillis = millis();
    toggle = !toggle;
    digitalWrite(PIN_LED_BUILTIN, toggle);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("ReMEDer starting up...");

  strip.begin();
  strip.show();

  pinMode(PIN_LED_BUILTIN, OUTPUT);
  pinMode(PIN_LED_RESET_BUTTON, OUTPUT);

  buttonReset.begin();
  buttonSelect.begin();
  buttonPrev.begin();
  buttonNext.begin();

  SetupRTC();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed."));
    Error();
  }
  else
  {
    Serial.println(F("SSD1306 allocated."));
  }

  LoadEEPROMData();
}

void loop()
{

  static unsigned long displayTimeoutMillis;

  BlinkOnboardLED();

  bool updateFlag = false;
  if (ProcessControlButtons())
  {
    updateFlag = true;
    displayOnFlag = true;
    displayTimeoutMillis = millis();

    // Check if time was updated by the user.
    static int oldTimeHour, oldTimeMinute;
    if (oldTimeHour != timeHour || oldTimeMinute != timeMinute)
    {
      oldTimeHour = timeHour;
      oldTimeMinute = timeMinute;
      Rtc.SetDateTime(RtcDateTime(2020, 1, 1, timeHour, timeMinute, 0));
      Serial.println("Saving time data to RTC.");
    }
  }

  // Turn off display after timeout.
  if ((displayTimeoutMillis + 10000) < millis())
  {
    displayTimeoutMillis = millis();
    displayOnFlag = false;
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  if (displayOnFlag)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
    UpdateDisplay(updateFlag);
  }

  // Check if time has updated.
  if (Rtc.IsDateTimeValid())
  {
    RtcDateTime dateTime = Rtc.GetDateTime();
    timeHour = dateTime.Hour();
    timeMinute = dateTime.Minute();

    // Trigger alarm only once upon time clocking into an alarm value.
    static int oldTimeHour, oldTimeMinute;
    if (oldTimeHour != timeHour || oldTimeMinute != timeMinute)
    {
      oldTimeHour = timeHour;
      oldTimeMinute = timeMinute;

      // Check for alarm trigger.
      for (int i = 0; i < numAlarms; i++)
      {
        if (timeHour == alarms[i].hour && timeMinute == alarms[i].minute)
        {
          indicatorOn = true;          
        }
      }
    }
  }
  else
  {
    // RTC error, likely bad battery.
    Error();
  }

  if (ProcessResetButton())
  {
    indicatorOn = false;
  }

  // Show alarm indicator when activated by the alarm or
  // when the user is interacting with certain menu items.
  if (displayOnFlag)
  {
    if (selectedMenuItem == COLOR || selectedMenuItem == PATTERN || selectedMenuItem == SPEED)
    {
      ProcessIndicator(true);
    }
    else
    {
      ProcessIndicator(false);
    }
  }
  else
  {
    ProcessIndicator(indicatorOn);
    analogWrite(PIN_LED_RESET_BUTTON, indicatorOn ? 127 : 0);
  }

  SaveEEPROMData();
}