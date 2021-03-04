/*
	ReMEDer2
	Reuben Strangelove
	Winter 2020

  A visual reminder to take your medicine.

	MCU: 
		Arduino Mini (AtMega328p)
  RTC:
    DS1307
	LCD: 
		SSD1306 OLED 128 x 32
	LEDs: 
		Neopixel strip  
*/

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RtcDS1307.h> // RTC Library: https://github.com/Makuna/Rtc
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <JC_Button.h>      // https://github.com/JChristensen/JC_Button
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

const int numPixels = 7;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(numPixels, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

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

const int maxNumAlarms = 6;
int selectedAlarm;
struct Alarm
{
  byte hour;
  byte minute;
};

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

const char *speedText[5] = {"Slow", "Medium", "Fast"};
enum Speeds
{
  SLOW,
  MEDIUM,
  FAST,
  MAX_SPEED
};

//
struct UserParams
{
  int color;
  int pattern;
  int speed;
  int numAlarms;
  Alarm alarms[maxNumAlarms];
} userParams;

///////////////////////////////////////////////////////////////////////////////

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
  if (userParams.color == RED)
  {
    strip.fill(strip.Color(255, 0, 0), 0, strip.numPixels());
  }
  else if (userParams.color == GREEN)
  {
    strip.fill(strip.Color(0, 255, 0), 0, strip.numPixels());
  }
  else if (userParams.color == BLUE)
  {
    strip.fill(strip.Color(0, 0, 255), 0, strip.numPixels());
  }
  else if (userParams.color == RANDOM)
  {
    static byte wheelPos = 0;
    if (newRandomColorFlag)
    {
      newRandomColorFlag = false;
      wheelPos = random(0, 256);
    }
    strip.fill(Wheel(wheelPos), 0, strip.numPixels());
  }
  else if (userParams.color == RAINBOW)
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

  if (userParams.pattern == FLASH)
  {
    static bool toggleFlag = true;
    static unsigned long last = millis();

    unsigned int delay = userParams.speed == 0 ? 2000 : userParams.speed == 1 ? 1000
                                                    : userParams.speed == 2   ? 500
                                                                              : 100;
    if (millis() - last > delay)
    {
      last = millis();
      toggleFlag = !toggleFlag;
      newRandomColorFlag = true;
    }
    strip.setBrightness(toggleFlag ? 255 : 0);
  }
  else if (userParams.pattern == SINWAVE)
  {
    static int sinValue = 0;
    static unsigned long last = millis();

    unsigned int delay = userParams.speed == 0 ? 10 : userParams.speed == 1 ? 5
                                                  : userParams.speed == 2   ? 1
                                                                            : 0;
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
  else if (userParams.pattern == STROBE)
  {
    static unsigned long last = millis();
    static bool toggleFlag = true;

    unsigned int delay = userParams.speed == 0 ? 3000 : userParams.speed == 1 ? 1500
                                                    : userParams.speed == 2   ? 500
                                                                              : 100;
    unsigned int strobeDelay = toggleFlag ? 250 : delay;
    if (millis() - last > strobeDelay)
    {
      last = millis();
      newRandomColorFlag = true;
      toggleFlag = !toggleFlag;
    }
    strip.setBrightness(toggleFlag ? 255 : 0);
  }
  else if (userParams.pattern == SPARKLE)
  {
    static byte index = 0;
    static unsigned long last = millis();

    unsigned int delay = userParams.speed == 0 ? 1000 : userParams.speed == 1 ? 500
                                                    : userParams.speed == 2   ? 200
                                                                              : 200;
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
  else if (userParams.pattern == CHASE)
  {
    static byte index = 0;
    static unsigned long last = millis();

    unsigned int delay = userParams.speed == 0 ? 500 : userParams.speed == 1 ? 250
                                                   : userParams.speed == 2   ? 100
                                                                             : 100;
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
      if (selectedAlarm > userParams.numAlarms)
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
      if (userParams.numAlarms == 1)
      {
        userParams.numAlarms = maxNumAlarms;
      }
      else
      {
        userParams.numAlarms--;
      }
    }
    else if (selectedMenuItem == ALARM_HOUR)
    {
      if (userParams.alarms[selectedAlarm - 1].hour == 0)
      {
        userParams.alarms[selectedAlarm - 1].hour = 23;
      }
      else
      {
        userParams.alarms[selectedAlarm - 1].hour--;
      }
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      if (userParams.alarms[selectedAlarm - 1].minute == 0)
      {
        userParams.alarms[selectedAlarm - 1].minute = 59;
      }
      else
      {
        userParams.alarms[selectedAlarm - 1].minute--;
      }
    }
    else if (selectedMenuItem == COLOR)
    {
      if (userParams.color == 0)
      {
        userParams.color = MAX_COLOR - 1;
      }
      else
      {
        userParams.color--;
      }
    }
    else if (selectedMenuItem == PATTERN)
    {
      if (userParams.pattern == 0)
      {
        userParams.pattern = MAX_PATTERN - 1;
      }
      else
      {
        userParams.pattern--;
      }
    }
    else if (selectedMenuItem == SPEED)
    {
      if (userParams.speed == 0)
      {
        userParams.speed = MAX_SPEED - 1;
      }
      else
      {
        userParams.speed--;
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
      userParams.numAlarms++;
      if (userParams.numAlarms >= maxNumAlarms)
      {
        userParams.numAlarms = 1;
      }
    }
    else if (selectedMenuItem == ALARM_HOUR)
    {
      userParams.alarms[selectedAlarm - 1].hour++;
      if (userParams.alarms[selectedAlarm - 1].hour > 59)
      {
        userParams.alarms[selectedAlarm - 1].hour = 0;
      }
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      userParams.alarms[selectedAlarm - 1].minute++;
      if (userParams.alarms[selectedAlarm - 1].minute > 59)
      {
        userParams.alarms[selectedAlarm - 1].minute = 0;
      }
    }
    else if (selectedMenuItem == COLOR)
    {
      userParams.color++;
      if (userParams.color >= MAX_PATTERN)
      {
        userParams.color = 0;
      }
    }
    else if (selectedMenuItem == PATTERN)
    {
      userParams.pattern++;
      if (userParams.pattern >= MAX_PATTERN)
      {
        userParams.pattern = 0;
      }
    }
    else if (selectedMenuItem == SPEED)
    {
      userParams.speed++;
      if (userParams.speed >= MAX_SPEED)
      {
        userParams.speed = 0;
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
  if (millis() - flashMillis > flashDelay)  
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
        display.println(F("Time"));
      }
      else if (selectedMenuItem == NUMALARMS)
      {
        display.println(F("No. Alarms"));
      }
      else if (selectedMenuItem == ALARM_HOUR || selectedMenuItem == ALARM_MIN)
      {
        sprintf(buf, "Alarm: %u", selectedAlarm);
        display.println(buf);
      }
      else if (selectedMenuItem == COLOR)
      {
        display.println(F("Color"));
      }
      else if (selectedMenuItem == PATTERN)
      {
        display.println(F("Pattern"));
      }
      else if (selectedMenuItem == SPEED)
      {
        display.println(F("Speed"));
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
      sprintf(buf, "%u        ", userParams.numAlarms);
    }
    else if (selectedMenuItem == ALARM_HOUR)
    {
      if (displayValue)
        sprintf(buf, "%02u:%02u", userParams.alarms[selectedAlarm - 1].hour, userParams.alarms[selectedAlarm - 1].minute);
      else
        sprintf(buf, "  :%02u", userParams.alarms[selectedAlarm - 1].minute);
    }
    else if (selectedMenuItem == ALARM_MIN)
    {
      if (displayValue)
        sprintf(buf, "%02u:%02u", userParams.alarms[selectedAlarm - 1].hour, userParams.alarms[selectedAlarm - 1].minute);
      else
        sprintf(buf, "%02u:  ", userParams.alarms[selectedAlarm - 1].hour);
    }
    else if (selectedMenuItem == COLOR)
    {
      if (displayValue)
      {
        sprintf(buf, "%-9s\n", colorText[userParams.color]);
        display.println(buf);
      }
      else
        display.println(F("         "));
    }
    else if (selectedMenuItem == PATTERN)
    {
      if (displayValue)
      {
        sprintf(buf, "%-9s\n", patternText[userParams.pattern]);
        display.println(buf);
      }
      else
        display.println(F("         "));
    }
    else if (selectedMenuItem == SPEED)
    {
      if (displayValue)
      {
        sprintf(buf, "%-9s\n", speedText[userParams.speed]);
        display.println(buf);
      }
      else
        display.println(F("         "));
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
      Serial.println(F("RTC lost confidence in the DateTime!"));
      Rtc.SetDateTime(compiled);
    }
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println(F("RTC was not actively running, starting now"));
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println(F("RTC is older than compile time!  (Updating DateTime)"));
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled)
  {
    Serial.println(F("RTC is newer than compile time. (this is expected)"));
  }
  else if (now == compiled)
  {
    Serial.println(F("RTC is the same as compile time! (not expected but all is fine)"));
  }

  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low);

  Serial.println(F("RTC setup finished."));
}

void LoadEEPROMData()
{
  EEPROM.get(0, userParams);

  // Check if EEPROM data has not not been initiated.
  for (int i = 0; i < maxNumAlarms; i++)
  {
    if (userParams.alarms[i].hour == 255)
    {
      userParams.alarms[i].hour = 0;
    }
    if (userParams.alarms[i].minute == 255)
    {
      userParams.alarms[i].minute = 0;
    }
  }

  if (userParams.color == 255)
  {
    userParams.color = 0;
  }
  if (userParams.pattern == 255)
  {
    userParams.pattern = 0;
  }
  if (userParams.speed == 255)
  {
    userParams.speed = 0;
  }
  if (userParams.numAlarms == 255)
  {
    userParams.numAlarms = 1;
  }
}

void SaveEEPROMData()
{
  EEPROM.put(0, userParams);
}

void BlinkOnboardLED()
{
  static unsigned long builtinLedMillis;
  static bool toggle;
  unsigned int delay = toggle ? 100 : 900;
  if (millis() - builtinLedMillis > delay)
  {
    builtinLedMillis = millis();
    toggle = !toggle;
    digitalWrite(PIN_LED_BUILTIN, toggle);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println(F("ReMEDer starting up..."));

  strip.begin();
  strip.show();

  pinMode(PIN_LED_BUILTIN, OUTPUT);
  pinMode(PIN_LED_RESET_BUTTON, OUTPUT);

  buttonReset.begin();
  buttonSelect.begin();
  buttonPrev.begin();
  buttonNext.begin();

  SetupRTC();

  delay(1000);

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
      Serial.println(F("Saving time data to RTC."));
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
      for (int i = 0; i < userParams.numAlarms; i++)
      {
        if (timeHour == userParams.alarms[i].hour && timeMinute == userParams.alarms[i].minute)
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