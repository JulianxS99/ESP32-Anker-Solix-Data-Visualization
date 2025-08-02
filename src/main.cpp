/*
  -----------------------------------------------------------------------------
  Project: Anker Solix 2 Balcony Power Plant Monitor
  Target:  ESP32‑2432S032C display module

  Description:
  This program runs on a standalone ESP32 with an integrated 3.2‑inch
  320 × 240 pixel colour TFT display (model ESP32‑2432S032C).  The board uses
  a dual‑core ESP32 microcontroller with integrated Wi‑Fi and Bluetooth and
  features an on‑board ST7789 LCD controller and capacitive touch
  interface.  The display resolution is 240 × 320 pixels with a 65‑k colour gamut.

  When connected to your home network the ESP32 retrieves energy data from
  either the Anker Solix cloud or a local smart‑meter and presents the daily
  generation and consumption curves on the TFT display.  Numerical values
  summarising the current battery charge (in %), the solar production of the
  current day and the electrical consumption measured by the smart‑meter are
  shown below the graph.  All text is printed in English.  The data
  acquisition is performed directly from the ESP32 without the need for an
  intermediate server.

  Two operation modes are supported:
    1. Anker cloud mode (``MODE_ANKER_CLOUD``): the firmware logs into the
       Anker Solix cloud service using your account credentials and fetches
       generation, consumption and battery information.  You must supply your
       account details and API endpoints in ``secrets.h``.  Please be aware
       that the official Anker API requires a valid account and may change
       without notice; consult the upstream documentation for details.

    2. Local smart‑meter mode (``MODE_LOCAL_SMARTMETER``): the firmware
       queries a local smart‑meter (for example, an Anker smart‑meter or
       another meter with a REST interface) running on your LAN.  The
       host IP/hostname, endpoints and optional authentication token must be
       configured in ``secrets.h``.  The smart‑meter is expected to return
       JSON data with instantaneous and historical values.

  The code is thoroughly commented to explain each step.  To use this
  firmware you must install the following Arduino libraries:
    • ``TFT_eSPI`` by Bodmer — used for driving the ST7789 display.  In
      ``TFT_eSPI/User_Setup.h`` enable the appropriate driver (e.g.
      ``#define ST7789_DRIVER``) and set the display resolution to 240 × 320.
    • ``ArduinoJson`` by Benoît Blanchon — used for parsing JSON returned by
      the cloud or smart‑meter.

  Place your Wi‑Fi and API credentials into ``src/secrets.h``.  The
  ``secrets_example.h`` file is provided as a template.

  -----------------------------------------------------------------------------
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>

// Optional touch support.  Define HAS_TOUCH to 1 and install a GT911 touch
// library (e.g. https://github.com/alex-code/GT911) to enable on‑screen
// refresh via a capacitive touch button.  When HAS_TOUCH is 0 the button is
// still drawn but no touch events are processed.
#ifndef HAS_TOUCH
#define HAS_TOUCH 0
#endif
#if HAS_TOUCH
#include <GT911.h>
GT911 touch;
#endif
#include <vector>

#include "secrets.h"

/*
 * Configuration constants.  Adjust these values to fine‑tune the behaviour
 * of the display and data acquisition.
 */
constexpr uint32_t REFRESH_INTERVAL_MS = 5UL * 60UL * 1000UL; // update every 5 minutes
constexpr int POINTS_PER_DAY = 24;                            // number of samples per day (hourly)

// Enumeration for operation mode.  Select MODE_ANKER_CLOUD to use the
// Anker cloud API or MODE_LOCAL_SMARTMETER to fetch data from your local
// smart‑meter.  You can switch at run time by changing currentMode.
enum class Mode { MODE_ANKER_CLOUD, MODE_LOCAL_SMARTMETER };

// Global objects
TFT_eSPI tft = TFT_eSPI();

// GPIO pin used to control the TFT backlight.  Some boards keep the
// backlight off after reset which results in a black screen until the pin
// is driven high.  Adjust the pin number if your hardware differs.
#ifndef TFT_BL
#define TFT_BL 27
#endif
WiFiClient wifiClient;

// Current operation mode; default to Anker cloud.  You may override this
// value in setup() by reading from non‑volatile storage or by using a
// configuration button.
Mode currentMode = Mode::MODE_ANKER_CLOUD;

// Geometry for the on‑screen refresh button.  Modify these values to
// reposition the button.  Coordinates are relative to the rotated
// (landscape) display.
constexpr int REFRESH_BTN_X = 230;
constexpr int REFRESH_BTN_Y = 200;
constexpr int REFRESH_BTN_W = 80;
constexpr int REFRESH_BTN_H = 30;

// Human readable timestamp of the last successful update.  It is
// initialised with a placeholder and updated after each successful fetch.
String lastUpdateStr = String("--:--:--");

// Forward declaration for timestamp helper
void updateTimestamp();

// Forward declarations
bool fetchAnkerData(float &batteryPercent, float &dailyGeneration,
                    float &dailyConsumption,
                    std::vector<float> &generationCurve,
                    std::vector<float> &consumptionCurve);
bool fetchSmartmeterData(float &batteryPercent, float &dailyGeneration,
                         float &dailyConsumption,
                         std::vector<float> &generationCurve,
                         std::vector<float> &consumptionCurve);
void drawGraph(const std::vector<float> &genData,
               const std::vector<float> &consData);
void drawNumbers(float batteryPercent, float dailyGeneration,
                 float dailyConsumption);
void showMessage(const char *msg);
void showBootText(const char *line1, const char *line2 = nullptr);

/*
 * Setup function runs once at boot.  It initialises the serial port,
 * display and Wi‑Fi connection.
 */
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting Setup");

  // Initialise the TFT display
  tft.init();
  tft.setRotation(1); // landscape orientation (320×240)
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

#ifdef TFT_BL
  // Turn on the backlight.  Use PWM via LEDC so brightness can be adjusted
  // later if desired.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  ledcSetup(0, 2000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255); // full brightness
#endif

  // Brief boot message before showing the splash screen
  showBootText("Starting...");
  delay(1000);

  // Display splash screen
  tft.drawString("Anker Solix Monitor", tft.width() / 2, tft.height() / 2 - 20);
  tft.drawString("Connecting to WiFi ...", tft.width() / 2, tft.height() / 2 + 10);

  // Connect to Wi‑Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    ++attempt;
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    showMessage("WiFi connection failed");
    return;
  }
  Serial.println("\nConnected to WiFi: '" WIFI_SSID "'");
  
  // tft.fillScreen(TFT_BLACK);
  // showMessage("Connected to WiFi: '" WIFI_SSID "'");
  tft.drawString("Connected to WiFi: '" WIFI_SSID "'", tft.width() / 2, tft.height() / 2 + 10);
  delay(10000);

  // Configure SNTP to obtain the current time.  This is used to display
  // the last update timestamp.  The offset and daylight parameters are set
  // to zero because relative time (UTC) suffices for display purposes.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  // Wait briefly until the time is set.  The epoch must be greater than
  // 1970‑01‑02 to indicate that a valid time has been acquired.  If
  // acquisition fails within ~5 seconds the loop will continue and the
  // timestamp will remain unset until the next fetch.
  time_t nowT = time(nullptr);
  uint8_t waitCount = 0;
  while (nowT < 100000 && waitCount < 50) {
    delay(100);
    nowT = time(nullptr);
    ++waitCount;
  }

#if HAS_TOUCH
  // Initialise the GT911 capacitive touch controller.  The driver uses the
  // default I2C bus and will configure the INT and RST pins if available on
  // your board.  Consult the library documentation if you need to specify
  // custom pins.
  touch.begin();
#endif
}

/*
 * Main loop.  At each iteration the firmware attempts to fetch fresh data
 * from the selected source and updates the display accordingly.  A
 * refresh timer prevents unnecessary network traffic.
 */
void loop() {
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();
  bool forceRefresh = false;
#if HAS_TOUCH
  // Poll the touch controller; if a touch occurs within the refresh
  // button area we schedule an immediate refresh
  uint8_t touches = touch.touched(GT911_MODE_POLLING);
  if (touches) {
    GTPoint *points = touch.getPoints();
    for (uint8_t i = 0; i < touches; ++i) {
      // Convert portrait coordinates (x,y) to our landscape orientation
      int16_t px = points[i].y;
      int16_t py = 320 - points[i].x;
      if (px >= REFRESH_BTN_X && px <= REFRESH_BTN_X + REFRESH_BTN_W &&
          py >= REFRESH_BTN_Y && py <= REFRESH_BTN_Y + REFRESH_BTN_H) {
        forceRefresh = true;
        break;
      }
    }
  }
#endif
  if (now - lastUpdate >= REFRESH_INTERVAL_MS || lastUpdate == 0 || forceRefresh) {
    lastUpdate = now;
    float batteryPercent = NAN;
    float dailyGen = NAN;
    float dailyCons = NAN;
    std::vector<float> genCurve(POINTS_PER_DAY, 0.0f);
    std::vector<float> consCurve(POINTS_PER_DAY, 0.0f);
    bool ok = false;
    if (currentMode == Mode::MODE_ANKER_CLOUD) {
      ok = fetchAnkerData(batteryPercent, dailyGen, dailyCons, genCurve, consCurve);
    } else {
      ok = fetchSmartmeterData(batteryPercent, dailyGen, dailyCons, genCurve, consCurve);
    }
    if (ok) {
      // Update timestamp of last successful fetch
      updateTimestamp();
      // Redraw the entire screen
      tft.fillScreen(TFT_BLACK);
      drawGraph(genCurve, consCurve);
      drawNumbers(batteryPercent, dailyGen, dailyCons);
    } else {
      showMessage("Data fetch error");
      // tft.drawString("Hallo Welt!", 20, 50);
    }
  }
  // Allow the CPU to rest between refreshes
  delay(100);
}

/*
 * Show a centred message on the screen.  This helper function clears the
 * screen and displays a single line of text for error or status messages.
 */
void showMessage(const char *msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
}

/*
 * Display a simple boot screen with one or two lines of centred text.  This
 * is used during startup before the main splash screen appears.
 */
void showBootText(const char *line1, const char *line2) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int centerY = tft.height() / 2;
  if (line2) {
    tft.drawString(line1, tft.width() / 2, centerY - 10);
    tft.drawString(line2, tft.width() / 2, centerY + 10);
  } else {
    tft.drawString(line1, tft.width() / 2, centerY);
  }
}

/*
 * Draw the daily generation and consumption curves on the screen.  The
 * function scales the values to fit within the graph area and draws axes
 * and legends.  The time axis spans 24 hours with one sample per hour.
 */
void drawGraph(const std::vector<float> &genData,
               const std::vector<float> &consData) {
  // Define graph geometry
  const int x0 = 10;
  const int y0 = 20;
  const int graphWidth = 300;
  const int graphHeight = 130;
  const uint16_t colourGen = TFT_GREEN;
  const uint16_t colourCons = TFT_RED;

  // Draw background for graph
  tft.fillRect(x0 - 2, y0 - 2, graphWidth + 4, graphHeight + 4, TFT_DARKGREY);
  tft.fillRect(x0, y0, graphWidth, graphHeight, TFT_BLACK);

  // Determine max value for scaling (avoid division by zero)
  float maxVal = 0.0f;
  for (size_t i = 0; i < genData.size(); ++i) {
    maxVal = max(maxVal, genData[i]);
    maxVal = max(maxVal, consData[i]);
  }
  if (maxVal < 1.0f) {
    maxVal = 1.0f;
  }

  // Draw axes
  tft.drawRect(x0, y0, graphWidth, graphHeight, TFT_LIGHTGREY);
  // Draw vertical grid lines and time labels every 6 hours
  for (int i = 0; i <= 24; i += 6) {
    int x = x0 + (graphWidth * i) / (POINTS_PER_DAY);  // removed -1 to align graph
    tft.drawLine(x, y0, x, y0 + graphHeight, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char label[4];
    sprintf(label, "%02d", i);
    tft.drawString(label, x, y0 + graphHeight + 2);
  }
  // Draw horizontal grid lines at 0%, 25%, 50%, 75%, 100%
  for (int i = 0; i <= 4; ++i) {
    int y = y0 + graphHeight - (graphHeight * i) / 4;
    tft.drawLine(x0, y, x0 + graphWidth, y, TFT_DARKGREY);
    // Y‑axis labels (percentage of maximum power)
    char label[6];
    sprintf(label, "%3d%%", i * 25);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(label, x0 - 30, y - 5);
  }
  // Draw generation and consumption curves
  int prevX = x0;
  int prevYGen = y0 + graphHeight - (genData[0] / maxVal) * graphHeight;
  int prevYCons = y0 + graphHeight - (consData[0] / maxVal) * graphHeight;
  for (int i = 1; i < POINTS_PER_DAY; ++i) {
    int x = x0 + (graphWidth * i) / (POINTS_PER_DAY);
    int yGen = y0 + graphHeight - (genData[i] / maxVal) * graphHeight;
    int yCons = y0 + graphHeight - (consData[i] / maxVal) * graphHeight;
    tft.drawLine(prevX, prevYGen, x, yGen, colourGen);
    tft.drawLine(prevX, prevYCons, x, yCons, colourCons);
    prevX = x;
    prevYGen = yGen;
    prevYCons = yCons;
  }
  // Draw legend
  int legendY = y0 - 10;
  tft.fillRect(x0, legendY - 8, 120, 16, TFT_BLACK);
  tft.fillRect(x0 + 2, legendY - 6, 10, 4, colourGen);
  tft.drawString("Generation", x0 + 16, legendY - 10);
  tft.fillRect(x0 + 80, legendY - 6, 10, 4, colourCons);
  tft.drawString("Consumption", x0 + 94, legendY - 10);
}

/*
 * Draw textual information (battery %, daily generation and consumption)
 * beneath the graph.  The values are formatted with one decimal place.  If
 * a value is NaN it is displayed as "--".
 */
void drawNumbers(float batteryPercent, float dailyGeneration,
                 float dailyConsumption) {
  int startY = 170;
  int colX = 10;
  int rowHeight = 20;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  // Battery state
  tft.drawString("Battery:", colX, startY);
  if (isnan(batteryPercent)) {
    tft.drawString("-- %", colX + 120, startY);
  } else {
    char buf[16];
    sprintf(buf, "%5.1f %%", batteryPercent);
    tft.drawString(buf, colX + 120, startY);
  }
  // Daily generation
  tft.drawString("Generated:", colX, startY + rowHeight);
  if (isnan(dailyGeneration)) {
    tft.drawString("-- kWh", colX + 120, startY + rowHeight);
  } else {
    char buf[16];
    sprintf(buf, "%5.2f kWh", dailyGeneration);
    tft.drawString(buf, colX + 120, startY + rowHeight);
  }
  // Daily consumption
  tft.drawString("Consumed:", colX, startY + 2 * rowHeight);
  if (isnan(dailyConsumption)) {
    tft.drawString("-- kWh", colX + 120, startY + 2 * rowHeight);
  } else {
    char buf[16];
    sprintf(buf, "%5.2f kWh", dailyConsumption);
    tft.drawString(buf, colX + 120, startY + 2 * rowHeight);
  }
  tft.setTextSize(1);

  // Draw the refresh button.  A dark grey filled rectangle with a light
  // border and white text forms the on‑screen button.  Users can tap
  // anywhere inside this area to trigger an immediate refresh when touch
  // support is enabled.
  tft.fillRect(REFRESH_BTN_X, REFRESH_BTN_Y, REFRESH_BTN_W, REFRESH_BTN_H,
               TFT_DARKGREY);
  tft.drawRect(REFRESH_BTN_X, REFRESH_BTN_Y, REFRESH_BTN_W, REFRESH_BTN_H,
               TFT_LIGHTGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(2);
  // Centre the text vertically within the button.  The string width is
  // approximated; adjust if you change the text.
  tft.drawString("Refresh", REFRESH_BTN_X + 6,
                  REFRESH_BTN_Y + (REFRESH_BTN_H - 16) / 2);
  tft.setTextSize(1);

  // Display the timestamp of the last successful update underneath the
  // button.  The label remains even if no updates have occurred yet.
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(String("Updated: ") + lastUpdateStr, 10,
                  REFRESH_BTN_Y + REFRESH_BTN_H + 4);
}

/*
 * Fetch energy data from the Anker Solix cloud.  The function returns true
 * on success and fills the provided references with the current battery
 * charge, daily generation and consumption (in kWh) together with hourly
 * arrays of generation and consumption power (in W).  The actual API
 * endpoints and authorisation tokens must be specified in ``secrets.h``.
 */
bool fetchAnkerData(float &batteryPercent, float &dailyGeneration,
                    float &dailyConsumption,
                    std::vector<float> &generationCurve,
                    std::vector<float> &consumptionCurve) {
  // Check that the user has configured the Anker API endpoints
  if (strlen(ANKER_AUTH_URL) == 0 || strlen(ANKER_ENERGY_URL) == 0) {
    Serial.println("Anker API endpoints are not configured");
    return false;
  }
  // Authenticate with the Anker cloud
  HTTPClient http;
  http.begin(wifiClient, ANKER_AUTH_URL);
  http.addHeader("Content-Type", "application/json");
  // Build JSON body for login; credentials defined in secrets.h
  StaticJsonDocument<256> loginDoc;
  loginDoc["userAccount"] = ANKER_USER;
  loginDoc["password"] = ANKER_PASSWORD;
  loginDoc["country"] = ANKER_COUNTRY;
  String loginBody;
  serializeJson(loginDoc, loginBody);
  int httpCode = http.POST(loginBody);
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Anker auth failed: %d\n", httpCode);
    http.end();
    return false;
  }
  // Parse authentication response
  String response = http.getString();
  http.end();
  StaticJsonDocument<1024> authDoc;
  DeserializationError err = deserializeJson(authDoc, response);
  if (err) {
    Serial.println("Failed to parse auth response");
    return false;
  }
  const char *token = authDoc["access_token"];
  if (!token) {
    Serial.println("No access token received");
    return false;
  }
  // Request daily energy data
  http.begin(wifiClient, ANKER_ENERGY_URL);
  http.addHeader("Authorization", String("Bearer ") + token);
  http.addHeader("Content-Type", "application/json");
  int energyCode = http.GET();
  if (energyCode != HTTP_CODE_OK) {
    Serial.printf("Energy request failed: %d\n", energyCode);
    http.end();
    return false;
  }
  String energyResponse = http.getString();
  http.end();
  // Parse energy response.  The expected JSON structure must be
  // documented by Anker.  Here we assume a structure similar to
  // {"battery_percent": 80.3, "daily_generation": 3.45,
  //  "daily_consumption": 2.10,
  //  "generation_curve": [24 floats ...],
  //  "consumption_curve": [24 floats ...] }.
  StaticJsonDocument<4096> energyDoc;
  err = deserializeJson(energyDoc, energyResponse);
  if (err) {
    Serial.println("Failed to parse energy response");
    return false;
  }
  // Extract numeric values; use NaN if not present
  batteryPercent = energyDoc["battery_percent"] | NAN;
  dailyGeneration = energyDoc["daily_generation"] | NAN;
  dailyConsumption = energyDoc["daily_consumption"] | NAN;
  // Fill curves
  JsonArray genArr = energyDoc["generation_curve"].as<JsonArray>();
  JsonArray consArr = energyDoc["consumption_curve"].as<JsonArray>();
  if (genArr.size() == POINTS_PER_DAY && consArr.size() == POINTS_PER_DAY) {
    for (int i = 0; i < POINTS_PER_DAY; ++i) {
      generationCurve[i] = genArr[i].as<float>();
      consumptionCurve[i] = consArr[i].as<float>();
    }
  } else {
    Serial.println("Invalid curve length; expected 24 values");
  }
  return true;
}

/*
 * Fetch energy data from a local smart‑meter.  The smart‑meter must provide
 * an HTTP API returning JSON with the same structure as described above.
 * Configure the host address and endpoint in ``secrets.h``.  Returns true
 * on success.
 */
bool fetchSmartmeterData(float &batteryPercent, float &dailyGeneration,
                         float &dailyConsumption,
                         std::vector<float> &generationCurve,
                         std::vector<float> &consumptionCurve) {
  if (strlen(SMARTMETER_HOST) == 0 || strlen(SMARTMETER_ENERGY_ENDPOINT) == 0) {
    Serial.println("Smart‑meter host or endpoint not configured");
    return false;
  }
  HTTPClient http;
  String url = String("http://") + SMARTMETER_HOST + SMARTMETER_ENERGY_ENDPOINT;
  http.begin(wifiClient, url);
  if (strlen(SMARTMETER_TOKEN) > 0) {
    http.addHeader("Authorization", String("Bearer ") + SMARTMETER_TOKEN);
  }
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Smart‑meter request failed: %d\n", httpCode);
    http.end();
    return false;
  }
  String response = http.getString();
  http.end();
  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.println("Failed to parse smart‑meter response");
    return false;
  }
  batteryPercent = doc["battery_percent"] | NAN;
  dailyGeneration = doc["daily_generation"] | NAN;
  dailyConsumption = doc["daily_consumption"] | NAN;
  JsonArray genArr = doc["generation_curve"].as<JsonArray>();
  JsonArray consArr = doc["consumption_curve"].as<JsonArray>();
  if (genArr.size() == POINTS_PER_DAY && consArr.size() == POINTS_PER_DAY) {
    for (int i = 0; i < POINTS_PER_DAY; ++i) {
      generationCurve[i] = genArr[i].as<float>();
      consumptionCurve[i] = consArr[i].as<float>();
    }
  } else {
    Serial.println("Invalid curve length from smart‑meter");
  }
  return true;
}

/*
 * Update the human‑readable timestamp string ``lastUpdateStr``.
 *
 * This helper uses the POSIX ``time`` and ``localtime`` functions provided
 * by the ESP32 standard C library to obtain the current UTC time.  The
 * timestamp is formatted as HH:MM:SS.  If the time has not been
 * synchronised via SNTP the function leaves ``lastUpdateStr`` unchanged
 * to indicate an unknown time.
 */
void updateTimestamp() {
  time_t nowT = time(nullptr);
  // Do not update if the epoch has not been set (less than 1970‑01‑02)
  if (nowT < 100000) {
    return;
  }
  struct tm timeInfo;
  if (!gmtime_r(&nowT, &timeInfo)) {
    return;
  }
  char buf[9];
  // Format as HH:MM:SS in 24‑hour notation
  if (strftime(buf, sizeof(buf), "%H:%M:%S", &timeInfo) > 0) {
    lastUpdateStr = String(buf);
  }
}