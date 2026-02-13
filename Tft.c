#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <SPI.h>

// -------------------- TOUCH PINS --------------------
#define T_CS   33
#define T_IRQ  27
#define T_MOSI 32
#define T_MISO 35
#define T_CLK  18

// -------------------- OBJECTS --------------------
TFT_eSPI tft;
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(T_CS, T_IRQ);
Preferences prefs;

// -------------------- GLOBAL VARIABLES --------------------
enum State { SCANNING, LIST, KEYPAD };
State currentState = SCANNING;

String selectedSSID = "";
String passwordBuffer = "";

int networkCount = 0;
bool scanStarted = false;
bool touchLocked = false;

// ============================================================
// SETUP
// ============================================================
void setup() {

  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  touchSPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);

  if (!touch.begin(touchSPI)) {
    Serial.println("Touch not found!");
    while (1);
  }

  touch.setRotation(1);

  prefs.begin("wifi-store", false);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  startScan();
}

// ============================================================
// LOOP
// ============================================================
void loop() {

  // ----------- NON-BLOCKING SCAN -----------
  if (currentState == SCANNING) {

    if (!scanStarted) {
      tft.fillScreen(TFT_BLACK);
      tft.drawCentreString("Scanning WiFi...", 160, 110, 2);
      WiFi.scanNetworks(true);
      scanStarted = true;
    }

    int status = WiFi.scanComplete();

    if (status >= 0) {
      networkCount = status;
      drawWifiList();
      currentState = LIST;
      scanStarted = false;
    }
  }

  // ----------- TOUCH HANDLING -----------
  if (touch.touched() && !touchLocked) {

    touchLocked = true;

    TS_Point p = touch.getPoint();

    // Adjust these if touch slightly misaligned
   int x = map(p.x, 421, 3798, 0, 320);
   int y = map(p.y, 3626, 362, 0, 240);


    if (currentState == LIST)
      handleListTouch(x, y);
    else if (currentState == KEYPAD)
      handleKeypadTouch(x, y);
  }

  if (!touch.touched()) {
    touchLocked = false;
  }
}

// ============================================================
// START SCAN
// ============================================================
void startScan() {
  currentState = SCANNING;
  scanStarted = false;
}

// ============================================================
// DRAW WIFI LIST
// ============================================================
void drawWifiList() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Select WiFi", 10, 5, 2);

  for (int i = 0; i < networkCount && i < 5; i++) {

    int yPos = 35 + (i * 40);

    tft.drawRect(5, yPos, 310, 35, TFT_WHITE);
    tft.drawString(WiFi.SSID(i), 15, yPos + 10, 2);
  }
}

// ============================================================
// HANDLE WIFI LIST TOUCH
// ============================================================
void handleListTouch(int x, int y) {

  for (int i = 0; i < networkCount && i < 5; i++) {

    int bx = 5;
    int by = 35 + (i * 40);
    int bw = 310;
    int bh = 35;

    if (x > bx && x < bx + bw &&
        y > by && y < by + bh) {

      selectedSSID = WiFi.SSID(i);
      passwordBuffer = "";

      drawKeypad();
      currentState = KEYPAD;
      return;
    }
  }
}

// ============================================================
// DRAW KEYPAD
// ============================================================
void drawKeypad() {

  tft.fillScreen(TFT_BLACK);
  tft.drawRect(10, 25, 300, 30, TFT_WHITE);

  const char* keys[4][3] = {
    {"3","2","1"},
    {"6","5","4"},
    {"9","8","7"},
    {"OK","0","CLR"}
  };

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {

      int x = 20 + (c * 95);
      int y = 65 + (r * 42);

      tft.drawRoundRect(x, y, 85, 38, 5, TFT_WHITE);
      tft.drawCentreString(keys[r][c], x + 42, y + 12, 2);
    }
  }
}

// ============================================================
// HANDLE KEYPAD TOUCH
// ============================================================
void handleKeypadTouch(int x, int y) {

  const char* keys[4][3] = {
    {"1","2","3"},
    {"4","5","6"},
    {"7","8","9"},
    {"CLR","0","OK"}
  };

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {

      int bx = 20 + (c * 95);
      int by = 65 + (r * 42);
      int bw = 85;
      int bh = 38;

      if (x > bx && x < bx + bw &&
          y > by && y < by + bh) {

        String val = keys[r][c];

        if (val == "CLR") {
          passwordBuffer = "";
        }
        else if (val == "OK") {
          connectToWifi(selectedSSID, passwordBuffer);
          return;
        }
        else {
          passwordBuffer += val;
        }

        tft.fillRect(12, 27, 296, 26, TFT_BLACK);
        tft.drawCentreString(passwordBuffer, 160, 32, 2);
      }
    }
  }
}

// ============================================================
// CONNECT TO WIFI
// ============================================================
void connectToWifi(String s, String p) {

  tft.fillScreen(TFT_BLACK);
  tft.drawCentreString("Connecting...", 160, 110, 2);

  WiFi.begin(s.c_str(), p.c_str());

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 10000) {
    delay(10);
  }

  if (WiFi.status() == WL_CONNECTED) {

    tft.fillScreen(TFT_GREEN);
    tft.drawCentreString("CONNECTED!", 160, 110, 2);

    prefs.putString("ssid", s);
    prefs.putString("pass", p);
  }
  else {

    tft.fillScreen(TFT_RED);
    tft.drawCentreString("FAILED!", 160, 110, 2);

    delay(1500);
    startScan();
  }
}