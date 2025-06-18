#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

TFT_eSPI tft = TFT_eSPI();

#define BLACK 0x0000
#define WHITE 0xFFFF
#define GRAY 0x8410
#define DARK_GRAY 0x4208
#define BLUE 0x001F
#define LIGHT_BLUE 0x051F
#define GREEN 0x07E0
#define RED 0xF800
#define ORANGE 0xFD20
#define YELLOW 0xFFE0
#define PURPLE 0x780F

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

const char *ssid = "**********";
const char *password = "**********";

uint16_t touchCalibration[4] = { 300, 3700, 300, 3700 };
bool touchCalibrated = false;

enum UIState {
  HOME_SCREEN,
  LIGHTS_SCREEN,
  CLIMATE_SCREEN,
  SECURITY_SCREEN,
  SETTINGS_SCREEN
};

UIState currentScreen = HOME_SCREEN;

struct DeviceStates {
  bool livingRoomLight = false;
  bool kitchenLight = false;
  bool bedroomLight = false;
  bool bathroomLight = false;
  int livingRoomBrightness = 100;
  int kitchenBrightness = 80;
  int bedroomBrightness = 60;
  int bathroomBrightness = 90;

  int temperature = 22;
  int targetTemp = 24;
  bool hvacOn = false;
  bool fanOn = false;
  int fanSpeed = 2;

  bool alarmArmed = false;
  bool doorLocked = true;
  bool motionDetected = false;
  bool windowOpen = false;

  int brightness = 80;
  bool darkMode = false;
} devices;

struct TouchPoint {
  int x, y;
  bool pressed;
};

TouchPoint lastTouch = { 0, 0, false };
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE = 200;

struct Button {
  int x, y, w, h;
  String label;
  uint16_t color;
  uint16_t textColor;
  bool enabled;
};

Button navButtons[5] = {
  { 10, 200, 55, 30, "Home", BLUE, WHITE, true },
  { 70, 200, 55, 30, "Lights", ORANGE, WHITE, true },
  { 130, 200, 55, 30, "Climate", GREEN, WHITE, true },
  { 190, 200, 55, 30, "Security", RED, WHITE, true },
  { 250, 200, 60, 30, "Settings", PURPLE, WHITE, true }
};

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  EEPROM.begin(512);

  runTouchCalibration();

  loadSettings();

  WiFi.begin(ssid, password);

  drawCurrentScreen();

  Serial.println("Smart Home Control System Initialized");
}

void loop() {
  handleTouch();
  updateSensors();
  delay(50);
}

void handleTouch() {
  uint16_t x, y;
  bool pressed = tft.getTouch(&x, &y);

  if (pressed && (millis() - lastTouchTime > TOUCH_DEBOUNCE)) {
    lastTouchTime = millis();

    int mappedX = map(x, touchCalibration[0], touchCalibration[1], 0, SCREEN_WIDTH);
    int mappedY = map(y, touchCalibration[2], touchCalibration[3], 0, SCREEN_HEIGHT);

    mappedX = constrain(mappedX, 0, SCREEN_WIDTH - 1);
    mappedY = constrain(mappedY, 0, SCREEN_HEIGHT - 1);

    lastTouch = { mappedX, mappedY, true };

    for (int i = 0; i < 5; i++) {
      if (isPointInButton(lastTouch.x, lastTouch.y, navButtons[i])) {
        currentScreen = (UIState)i;
        drawCurrentScreen();
        return;
      }
    }

    handleScreenTouch(lastTouch.x, lastTouch.y);
  }
}

bool isPointInButton(int x, int y, Button btn) {
  return (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h && btn.enabled);
}

void drawCurrentScreen() {
  tft.fillScreen(devices.darkMode ? BLACK : WHITE);

  switch (currentScreen) {
    case HOME_SCREEN:
      drawHomeScreen();
      break;
    case LIGHTS_SCREEN:
      drawLightsScreen();
      break;
    case CLIMATE_SCREEN:
      drawClimateScreen();
      break;
    case SECURITY_SCREEN:
      drawSecurityScreen();
      break;
    case SETTINGS_SCREEN:
      drawSettingsScreen();
      break;
  }

  drawNavigation();
}

void drawHomeScreen() {
  uint16_t bgColor = devices.darkMode ? BLACK : WHITE;
  uint16_t textColor = devices.darkMode ? WHITE : BLACK;
  uint16_t cardColor = devices.darkMode ? DARK_GRAY : GRAY;

  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.drawString("Smart Home", 10, 10);

  tft.setTextSize(1);
  tft.drawString("12:34 PM", 250, 15);

  drawStatusCard(10, 40, 90, 50, "Lights", devices.livingRoomLight ? "ON" : "OFF",
                 devices.livingRoomLight ? GREEN : RED, cardColor);
  drawStatusCard(110, 40, 90, 50, "Climate", String(devices.temperature) + "°C",
                 devices.hvacOn ? BLUE : GRAY, cardColor);
  drawStatusCard(210, 40, 90, 50, "Security", devices.alarmArmed ? "ARMED" : "OFF",
                 devices.alarmArmed ? RED : GREEN, cardColor);

  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.drawString("Quick Controls", 10, 100);

  drawToggleButton(10, 120, 80, 30, "All Lights", devices.livingRoomLight, ORANGE);
  drawToggleButton(100, 120, 80, 30, "HVAC", devices.hvacOn, BLUE);
  drawToggleButton(190, 120, 80, 30, "Lock Door", devices.doorLocked, RED);

  tft.drawString("Scenes", 10, 160);
  drawButton(10, 175, 60, 20, "Morning", YELLOW, BLACK);
  drawButton(80, 175, 60, 20, "Evening", PURPLE, WHITE);
  drawButton(150, 175, 60, 20, "Night", DARK_GRAY, WHITE);
  drawButton(220, 175, 60, 20, "Away", RED, WHITE);
}

void drawLightsScreen() {
  uint16_t textColor = devices.darkMode ? WHITE : BLACK;

  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.drawString("Lighting Control", 10, 10);

  drawLightControl(10, 40, "Living Room", devices.livingRoomLight, devices.livingRoomBrightness);
  drawLightControl(160, 40, "Kitchen", devices.kitchenLight, devices.kitchenBrightness);
  drawLightControl(10, 100, "Bedroom", devices.bedroomLight, devices.bedroomBrightness);
  drawLightControl(160, 100, "Bathroom", devices.bathroomLight, devices.bathroomBrightness);

  tft.setTextSize(1);
  tft.drawString("Master Controls", 10, 160);
  drawButton(10, 175, 60, 20, "All ON", GREEN, WHITE);
  drawButton(80, 175, 60, 20, "All OFF", RED, WHITE);
  drawButton(150, 175, 80, 20, "Dim All", ORANGE, WHITE);
  drawButton(240, 175, 60, 20, "Bright", YELLOW, BLACK);
}

void drawClimateScreen() {
  uint16_t textColor = devices.darkMode ? WHITE : BLACK;

  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.drawString("Climate Control", 10, 10);

  tft.setTextSize(3);
  tft.drawString(String(devices.temperature) + "°C", 50, 50);
  tft.setTextSize(1);
  tft.drawString("Current", 60, 80);

  tft.setTextSize(2);
  tft.drawString(String(devices.targetTemp) + "°C", 180, 60);
  tft.setTextSize(1);
  tft.drawString("Target", 190, 80);

  drawButton(140, 40, 30, 30, "+", GREEN, WHITE);
  drawButton(140, 75, 30, 30, "-", RED, WHITE);

  drawToggleButton(10, 110, 80, 30, "HVAC", devices.hvacOn, BLUE);
  drawToggleButton(100, 110, 80, 30, "Fan", devices.fanOn, LIGHT_BLUE);

  tft.setTextColor(textColor);
  tft.drawString("Fan Speed: " + String(devices.fanSpeed), 200, 120);
  drawButton(270, 110, 20, 15, "+", GREEN, WHITE);
  drawButton(270, 130, 20, 15, "-", RED, WHITE);

  tft.drawString("Modes", 10, 150);
  drawButton(10, 165, 50, 20, "Cool", BLUE, WHITE);
  drawButton(70, 165, 50, 20, "Heat", RED, WHITE);
  drawButton(130, 165, 50, 20, "Auto", GREEN, WHITE);
  drawButton(190, 165, 50, 20, "Off", GRAY, WHITE);
}

void drawSecurityScreen() {
  uint16_t textColor = devices.darkMode ? WHITE : BLACK;

  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.drawString("Security System", 10, 10);

  uint16_t alarmColor = devices.alarmArmed ? RED : GREEN;
  String alarmText = devices.alarmArmed ? "ARMED" : "DISARMED";
  drawStatusCard(10, 40, 120, 40, "Alarm", alarmText, alarmColor, DARK_GRAY);

  uint16_t lockColor = devices.doorLocked ? GREEN : RED;
  String lockText = devices.doorLocked ? "LOCKED" : "UNLOCKED";
  drawStatusCard(140, 40, 120, 40, "Door", lockText, lockColor, DARK_GRAY);

  tft.setTextSize(1);
  tft.drawString("Sensors", 10, 90);

  drawSensorStatus(10, 105, "Motion", devices.motionDetected, devices.motionDetected ? RED : GREEN);
  drawSensorStatus(90, 105, "Window", devices.windowOpen, devices.windowOpen ? ORANGE : GREEN);
  drawSensorStatus(170, 105, "Door", !devices.doorLocked, !devices.doorLocked ? RED : GREEN);

  tft.drawString("Controls", 10, 130);
  drawToggleButton(10, 145, 80, 25, "Arm System", devices.alarmArmed, RED);
  drawToggleButton(100, 145, 80, 25, "Lock Door", devices.doorLocked, BLUE);
  drawButton(190, 145, 80, 25, "Emergency", RED, WHITE);

  drawButton(10, 175, 60, 20, "Home", GREEN, WHITE);
  drawButton(80, 175, 60, 20, "Away", ORANGE, WHITE);
  drawButton(150, 175, 60, 20, "Sleep", PURPLE, WHITE);
  drawButton(220, 175, 60, 20, "Vacation", RED, WHITE);
}

void drawSettingsScreen() {
  uint16_t textColor = devices.darkMode ? WHITE : BLACK;

  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.drawString("Settings", 10, 10);

  tft.setTextSize(1);
  tft.drawString("Display", 10, 40);
  drawToggleButton(10, 55, 100, 25, "Dark Mode", devices.darkMode, PURPLE);

  tft.drawString("Brightness: " + String(devices.brightness) + "%", 120, 60);
  drawButton(220, 55, 20, 12, "+", GREEN, WHITE);
  drawButton(220, 70, 20, 12, "-", RED, WHITE);

  tft.drawString("Network", 10, 90);
  drawButton(10, 105, 80, 20, "WiFi Setup", BLUE, WHITE);
  drawButton(100, 105, 80, 20, "IP Config", LIGHT_BLUE, WHITE);

  tft.drawString("System", 10, 135);
  drawButton(10, 150, 60, 20, "Reset", RED, WHITE);
  drawButton(80, 150, 60, 20, "Update", GREEN, WHITE);
  drawButton(150, 150, 60, 20, "Backup", ORANGE, WHITE);
  drawButton(220, 150, 60, 20, "Info", GRAY, WHITE);

  tft.drawString("Version 1.0 - Smart Home UI", 10, 180);
}

void drawNavigation() {
  tft.fillRect(0, 195, SCREEN_WIDTH, 45, DARK_GRAY);

  for (int i = 0; i < 5; i++) {
    uint16_t btnColor = (currentScreen == i) ? navButtons[i].color : GRAY;
    drawButton(navButtons[i].x, navButtons[i].y, navButtons[i].w, navButtons[i].h,
               navButtons[i].label, btnColor, navButtons[i].textColor);
  }
}

void drawButton(int x, int y, int w, int h, String text, uint16_t color, uint16_t textColor) {
  tft.fillRoundRect(x, y, w, h, 3, color);
  tft.drawRoundRect(x, y, w, h, 3, WHITE);

  tft.setTextColor(textColor);
  tft.setTextSize(1);

  int textWidth = text.length() * 6;
  int textX = x + (w - textWidth) / 2;
  int textY = y + (h - 8) / 2;

  tft.drawString(text, textX, textY);
}

void drawToggleButton(int x, int y, int w, int h, String text, bool state, uint16_t activeColor) {
  uint16_t color = state ? activeColor : GRAY;
  uint16_t textColor = state ? WHITE : BLACK;

  drawButton(x, y, w, h, text, color, textColor);
}

void drawStatusCard(int x, int y, int w, int h, String title, String value, uint16_t valueColor, uint16_t bgColor) {
  tft.fillRoundRect(x, y, w, h, 5, bgColor);
  tft.drawRoundRect(x, y, w, h, 5, WHITE);

  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.drawString(title, x + 5, y + 5);

  tft.setTextColor(valueColor);
  tft.setTextSize(1);
  tft.drawString(value, x + 5, y + 20);
}

void drawLightControl(int x, int y, String room, bool state, int brightness) {
  uint16_t cardColor = devices.darkMode ? DARK_GRAY : GRAY;
  uint16_t textColor = devices.darkMode ? WHITE : BLACK;

  tft.fillRoundRect(x, y, 140, 50, 5, cardColor);
  tft.drawRoundRect(x, y, 140, 50, 5, WHITE);

  tft.setTextColor(textColor);
  tft.setTextSize(1);
  tft.drawString(room, x + 5, y + 5);

  drawToggleButton(x + 5, y + 20, 40, 20, state ? "ON" : "OFF", state, ORANGE);

  if (state) {
    tft.drawString(String(brightness) + "%", x + 50, y + 25);
    drawButton(x + 90, y + 20, 15, 10, "+", GREEN, WHITE);
    drawButton(x + 110, y + 20, 15, 10, "-", RED, WHITE);
  }
}

void drawSensorStatus(int x, int y, String sensor, bool active, uint16_t color) {
  tft.fillCircle(x + 5, y + 5, 5, color);
  tft.setTextColor(devices.darkMode ? WHITE : BLACK);
  tft.setTextSize(1);
  tft.drawString(sensor, x + 15, y);
}

void handleScreenTouch(int x, int y) {
  switch (currentScreen) {
    case HOME_SCREEN:
      handleHomeTouch(x, y);
      break;
    case LIGHTS_SCREEN:
      handleLightsTouch(x, y);
      break;
    case CLIMATE_SCREEN:
      handleClimateTouch(x, y);
      break;
    case SECURITY_SCREEN:
      handleSecurityTouch(x, y);
      break;
    case SETTINGS_SCREEN:
      handleSettingsTouch(x, y);
      break;
  }
}

void handleHomeTouch(int x, int y) {

  if (isPointInRect(x, y, 10, 120, 80, 30)) {
    devices.livingRoomLight = !devices.livingRoomLight;
    devices.kitchenLight = devices.livingRoomLight;
    devices.bedroomLight = devices.livingRoomLight;
    devices.bathroomLight = devices.livingRoomLight;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 100, 120, 80, 30)) {
    devices.hvacOn = !devices.hvacOn;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 190, 120, 80, 30)) {
    devices.doorLocked = !devices.doorLocked;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 10, 175, 60, 20)) {
    setMorningScene();
  } else if (isPointInRect(x, y, 80, 175, 60, 20)) {
    setEveningScene();
  } else if (isPointInRect(x, y, 150, 175, 60, 20)) {
    setNightScene();
  } else if (isPointInRect(x, y, 220, 175, 60, 20)) {
    setAwayScene();
  }
}

void handleLightsTouch(int x, int y) {

  if (isPointInRect(x, y, 15, 60, 40, 20)) {
    devices.livingRoomLight = !devices.livingRoomLight;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 100, 60, 15, 10) && devices.livingRoomLight) {
    devices.livingRoomBrightness = min(100, devices.livingRoomBrightness + 10);
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 120, 60, 15, 10) && devices.livingRoomLight) {
    devices.livingRoomBrightness = max(10, devices.livingRoomBrightness - 10);
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 165, 60, 40, 20)) {
    devices.kitchenLight = !devices.kitchenLight;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 250, 60, 15, 10) && devices.kitchenLight) {
    devices.kitchenBrightness = min(100, devices.kitchenBrightness + 10);
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 270, 60, 15, 10) && devices.kitchenLight) {
    devices.kitchenBrightness = max(10, devices.kitchenBrightness - 10);
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 10, 175, 60, 20)) {
    devices.livingRoomLight = devices.kitchenLight = devices.bedroomLight = devices.bathroomLight = true;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 80, 175, 60, 20)) {
    devices.livingRoomLight = devices.kitchenLight = devices.bedroomLight = devices.bathroomLight = false;
    drawCurrentScreen();
  }
}

void handleClimateTouch(int x, int y) {

  if (isPointInRect(x, y, 140, 40, 30, 30)) {
    devices.targetTemp = min(30, devices.targetTemp + 1);
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 140, 75, 30, 30)) {
    devices.targetTemp = max(15, devices.targetTemp - 1);
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 10, 110, 80, 30)) {
    devices.hvacOn = !devices.hvacOn;
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 100, 110, 80, 30)) {
    devices.fanOn = !devices.fanOn;
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 270, 110, 20, 15)) {
    devices.fanSpeed = min(5, devices.fanSpeed + 1);
    drawCurrentScreen();
  }

  else if (isPointInRect(x, y, 270, 130, 20, 15)) {
    devices.fanSpeed = max(1, devices.fanSpeed - 1);
    drawCurrentScreen();
  }
}

void handleSecurityTouch(int x, int y) {
  if (isPointInRect(x, y, 10, 145, 80, 25)) {
    devices.alarmArmed = !devices.alarmArmed;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 100, 145, 80, 25)) {
    devices.doorLocked = !devices.doorLocked;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 10, 175, 60, 20)) {  // Home
    devices.alarmArmed = false;
    devices.doorLocked = true;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 80, 175, 60, 20)) {  // Away
    devices.alarmArmed = true;
    devices.doorLocked = true;
    drawCurrentScreen();
  }
}

void handleSettingsTouch(int x, int y) {
  if (isPointInRect(x, y, 10, 55, 100, 25)) {
    devices.darkMode = !devices.darkMode;
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 220, 55, 20, 12)) {
    devices.brightness = min(100, devices.brightness + 10);
    drawCurrentScreen();
  } else if (isPointInRect(x, y, 220, 70, 20, 12)) {
    devices.brightness = max(10, devices.brightness - 10);
    drawCurrentScreen();
  }
}

bool isPointInRect(int x, int y, int rx, int ry, int rw, int rh) {
  return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

void setMorningScene() {
  devices.livingRoomLight = true;
  devices.kitchenLight = true;
  devices.livingRoomBrightness = 80;
  devices.kitchenBrightness = 90;
  devices.targetTemp = 22;
  devices.hvacOn = true;
  devices.alarmArmed = false;
  drawCurrentScreen();
}

void setEveningScene() {
  devices.livingRoomLight = true;
  devices.bedroomLight = true;
  devices.livingRoomBrightness = 60;
  devices.bedroomBrightness = 40;
  devices.targetTemp = 21;
  devices.hvacOn = true;
  drawCurrentScreen();
}

void setNightScene() {
  devices.livingRoomLight = false;
  devices.kitchenLight = false;
  devices.bedroomLight = true;
  devices.bedroomBrightness = 20;
  devices.targetTemp = 20;
  devices.alarmArmed = true;
  devices.doorLocked = true;
  drawCurrentScreen();
}

void setAwayScene() {
  devices.livingRoomLight = false;
  devices.kitchenLight = false;
  devices.bedroomLight = false;
  devices.bathroomLight = false;
  devices.hvacOn = false;
  devices.alarmArmed = true;
  devices.doorLocked = true;
  drawCurrentScreen();
}

void updateSensors() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    devices.temperature = random(20, 26);
    devices.motionDetected = random(0, 10) < 2;
    devices.windowOpen = random(0, 20) < 1;
    lastUpdate = millis();
  }
}

void loadSettings() {
  devices.darkMode = EEPROM.read(0);
  devices.brightness = EEPROM.read(1);
  if (devices.brightness == 255) devices.brightness = 80;
}

void saveSettings() {
  EEPROM.write(0, devices.darkMode);
  EEPROM.write(1, devices.brightness);
}

void runTouchCalibration() {
  Serial.println("Starting touch calibration...");

  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.drawString("Touch Calibration", 60, 20);

  tft.setTextSize(1);
  tft.drawString("Touch the circles accurately", 70, 50);
  tft.drawString("for best results", 110, 65);

  int calibPoints[4][2] = {
    { 20, 20 },
    { 300, 20 },
    { 300, 220 },
    { 20, 220 }
  };

  uint16_t touchReadings[4][2];

  for (int i = 0; i < 4; i++) {

    if (i > 0) {
      tft.fillCircle(calibPoints[i - 1][0], calibPoints[i - 1][1], 15, BLACK);
    }

    tft.fillCircle(calibPoints[i][0], calibPoints[i][1], 15, RED);
    tft.fillCircle(calibPoints[i][0], calibPoints[i][1], 5, WHITE);

    tft.fillRect(0, 100, SCREEN_WIDTH, 60, BLACK);
    tft.setTextColor(YELLOW);
    tft.setTextSize(1);
    String instruction = "Touch point " + String(i + 1) + " of 4";
    tft.drawString(instruction, 100, 110);
    tft.drawString("Press accurately on center", 80, 130);

    bool touched = false;
    while (!touched) {
      uint16_t x, y;
      if (tft.getTouch(&x, &y)) {
        touchReadings[i][0] = x;
        touchReadings[i][1] = y;
        touched = true;

        tft.fillCircle(calibPoints[i][0], calibPoints[i][1], 15, GREEN);
        delay(500);

        Serial.printf("Point %d: Screen(%d,%d) -> Touch(%d,%d)\n",
                      i + 1, calibPoints[i][0], calibPoints[i][1], x, y);
      }
      delay(50);
    }
  }

  touchCalibration[0] = (touchReadings[0][0] + touchReadings[3][0]) / 2;
  touchCalibration[1] = (touchReadings[1][0] + touchReadings[2][0]) / 2;
  touchCalibration[2] = (touchReadings[0][1] + touchReadings[1][1]) / 2;
  touchCalibration[3] = (touchReadings[2][1] + touchReadings[3][1]) / 2;

  touchCalibrated = true;

  tft.fillScreen(BLACK);
  tft.setTextColor(GREEN);
  tft.setTextSize(2);
  tft.drawString("Calibration", 90, 80);
  tft.drawString("Complete!", 100, 100);

  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.drawString("Touch anywhere to continue", 80, 140);

  Serial.println("Touch calibration completed");
  Serial.printf("Final calibration: xMin=%d, xMax=%d, yMin=%d, yMax=%d\n",
                touchCalibration[0], touchCalibration[1],
                touchCalibration[2], touchCalibration[3]);

  while (true) {
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
      delay(200);
      break;
    }
    delay(50);
  }
  tft.fillScreen(BLACK);
}