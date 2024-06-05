#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

// Define the I2C address of the OLED display
#define OLED_ADDRESS 0x3C

// Define speaker pin
#define SPEAKER_PIN 13

// Define button pins
#define BUTTON_SET 14
#define BUTTON_START_STOP 12

// Create an instance of the U8g2 library for I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Create an RTC instance
RTC_DS3231 rtc;

// Timer and alarm variables
bool timerRunning = false;
bool playSound = false;
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0; // in seconds
bool showClock = true;
bool settingTimer = false; // To track if we are setting the timer
unsigned long remainingTime = timerDuration;
void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize the display
  u8g2.begin();

  // Set the I2C address of the display
  u8g2.setI2CAddress(OLED_ADDRESS << 1); // Shift by 1 bit for Wire library

  // Initialize the RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Check if the RTC lost power and if so, set the time
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize buttons
  pinMode(BUTTON_SET, INPUT_PULLUP);
  pinMode(BUTTON_START_STOP, INPUT_PULLUP);
}


void loop() {
  DateTime now = rtc.now();

  // Handle button presses
  handleButtonPresses(now);

  // Draw the appropriate display (clock or timer)
  u8g2.clearBuffer();
  if (showClock) {
    drawClockDisplay(now);
  } else {
    drawTimerDisplay(now);
  }
  u8g2.sendBuffer();

  delay(5);
  if(playSound && remainingTime == 0){
    Serial.println("Remainng Time: " + String(remainingTime));
    playTimerSound();
    playSound = false;
  }
  // Wait for a short time before refreshing the display
  delay(100);
}

// Handle button presses and update state variables accordingly
void handleButtonPresses(DateTime now) {
  static bool lastButtonSetState = HIGH;
  static bool lastButtonStartStopState = HIGH;

  bool buttonSetState = digitalRead(BUTTON_SET);
  bool buttonStartStopState = digitalRead(BUTTON_START_STOP);

  if (buttonSetState == LOW && lastButtonSetState == HIGH) {
    if (showClock) {
      // Toggle to timer display
      showClock = false;
      settingTimer = true; // Enter timer setting mode
    } else if (settingTimer) {
      // Increase timer duration by 1 minute
      timerDuration += 60;
    } else {
      // Toggle back to clock display and reset timer
      showClock = true;
      timerDuration = 0;
      timerRunning = false;
    }
  }

  if (buttonStartStopState == LOW && lastButtonStartStopState == HIGH) {
    if (settingTimer) {
      // Exit timer setting mode and start timer
      settingTimer = false;
      timerRunning = true;
      timerStartTime = now.unixtime();
    } else {
      // Start/Stop button pressed
      if (timerRunning) {
        // Stop the timer
        timerRunning = false;
      } else {
        // Start the timer
        timerRunning = true;
        timerStartTime = now.unixtime();
      }
    }
  }

  lastButtonSetState = buttonSetState;
  lastButtonStartStopState = buttonStartStopState;
}

// Draw the clock display on the OLED screen
void drawClockDisplay(DateTime now) {
  char dateBuffer[11]; // Buffer for date: DD/MM/YYYY
  char timeBuffer[10];  // Buffer for time: HH:MM AM/PM

  snprintf(dateBuffer, sizeof(dateBuffer), "%02d/%02d/%04d", now.day(), now.month(), now.year());
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d %s", formatHour12(now.hour()), now.minute(), isPM(now.hour()) ? "PM" : "AM");

  // Set font for the date and day of the week
  u8g2.setFont(u8g2_font_VCR_OSD_mu);
  u8g2.drawStr(2, 18, dateBuffer);

  u8g2.setFont(u8g2_font_courB18_tf);
  u8g2.drawStr(2, 35, dayOfTheWeek(now.dayOfTheWeek()));

  // Set font for the time (larger size)
  u8g2.setFont(u8g2_font_fub20_tf);
  u8g2.drawStr(2, 63, timeBuffer);
}

// Draw the timer display on the OLED screen
void drawTimerDisplay(DateTime now) {
  if (settingTimer) {
    // Display the timer duration being set
    char timerBuffer[10];
    snprintf(timerBuffer, sizeof(timerBuffer), "%02lu:%02lu", timerDuration / 60, timerDuration % 60);
    u8g2.setFont(u8g2_font_fub20_tf);
    u8g2.drawStr(32, 63, timerBuffer);
  } else {
    // Timer logic
    unsigned long elapsedTime = 0;
    // unsigned long remainingTime = timerDuration;

    if (timerRunning) {
      elapsedTime = now.unixtime() - timerStartTime;
      remainingTime = timerDuration > elapsedTime ? timerDuration - elapsedTime : 0;
      if (remainingTime == 0) {
        // Timer finished
        timerRunning = false;
        playSound = true;
        timerDuration = 0;  // Reset the timer duration
      }
    }

    // Display the remaining time
    char timerBuffer[10];
    snprintf(timerBuffer, sizeof(timerBuffer), "%02lu:%02lu", remainingTime / 60, remainingTime % 60);
    u8g2.setFont(u8g2_font_fub20_tf);
    u8g2.drawStr(32, 63, timerBuffer);

    // Draw pie animation
    drawPieAnimation(remainingTime, timerDuration);
  }
}

// Convert 24-hour format to 12-hour format
int formatHour12(int hour24) {
  int hour12 = hour24;
  if (hour12 > 12) hour12 -= 12;
  if (hour12 == 0) hour12 = 12;
  return hour12;
}

// Check if the given hour is PM
bool isPM(int hour) {
  return hour >= 12;
}

// Return the day of the week as a string
const char* dayOfTheWeek(uint8_t day) {
  static const char* daysOfTheWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  return daysOfTheWeek[day];
}

// Draw pie animation to represent remaining time on the timer
void drawPieAnimation(unsigned long remainingTime, unsigned long totalTime) {
  int centerX = 64;
  int centerY = 22;
  int radius = 20;

  if (totalTime == 0) return;

  float percent = (float)remainingTime / totalTime;
  int endAngle = (360 * percent) - 90;

  for (int angle = -90; angle <= endAngle; angle++) {
    float radAngle = angle * 3.14159265 / 180.0; // Convert to radians
    int x1 = centerX + radius * cos(radAngle);
    int y1 = centerY + radius * sin(radAngle);
    int x2 = centerX + (radius - 1) * cos(radAngle);
    int y2 = centerY + (radius - 1) * sin(radAngle);
    u8g2.drawLine(centerX, centerY, x1, y1);
    u8g2.drawLine(centerX, centerY, x2, y2);
  }
}

