#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Pin Configuration ---
#define BUTTON_PIN 13 

// --- WiFi Configuration ---
const char* ssid = "KFON@Wi-Fi";
const char* password = "KFON@123";

// --- API & Time Configuration ---
const char* weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=11.018&longitude=76.175&current=temperature_2m,relative_humidity_2m,weather_code";
const char* f1Url = "https://api.openf1.org/v1/race_control?session_key=latest";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST (+5:30)
const int   daylightOffset_sec = 0;

// --- Global Variables ---
int screenState = 0; // 0 = Time, 1 = Weather, 2 = F1 Alerts
bool lastButtonState = HIGH;

unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 600000; // 10 minutes

unsigned long lastF1Update = 0;
const unsigned long f1UpdateInterval = 60000; // 1 minute (F1 alerts change fast)

// Weather Vars
float currentTemp = 0.0;
int currentHumidity = 0;
int currentWeatherCode = 0; 
String weatherStatus = "Fetching...";

// F1 Vars
String f1Message = "Fetching F1 Data...";
String f1Flag = "NONE";

// --- 16x16 Bitmap Icons (Stored in PROGMEM) ---
const unsigned char icon_sun [] PROGMEM = {
  0x03, 0xc0, 0x01, 0x80, 0x09, 0x90, 0x08, 0x10, 0x18, 0x18, 0x23, 0xc4, 0x47, 0xe2, 0x4f, 0xf2, 
  0x4f, 0xf2, 0x47, 0xe2, 0x23, 0xc4, 0x18, 0x18, 0x08, 0x10, 0x09, 0x90, 0x01, 0x80, 0x03, 0xc0
};

const unsigned char icon_cloud [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x0c, 0x30, 0x18, 0x18, 0x30, 0x0c, 
  0x20, 0x04, 0x60, 0x06, 0x40, 0x02, 0xc0, 0x03, 0x80, 0x01, 0xff, 0xff, 0x7f, 0xfe, 0x00, 0x00
};

const unsigned char icon_rain [] PROGMEM = {
  0x00, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x0c, 0x30, 0x18, 0x18, 0x30, 0x0c, 0x60, 0x06, 
  0xff, 0xff, 0x7f, 0xfe, 0x00, 0x00, 0x22, 0x22, 0x11, 0x11, 0x22, 0x22, 0x11, 0x11, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,20);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  fetchWeather();
  fetchF1Alerts();
}

void loop() {
  handleButton();

  // Background Tasks
  if (millis() - lastWeatherUpdate > weatherUpdateInterval) {
    fetchWeather();
    lastWeatherUpdate = millis();
  }

  if (millis() - lastF1Update > f1UpdateInterval) {
    fetchF1Alerts();
    lastF1Update = millis();
  }

  display.clearDisplay();
  
  if (screenState == 0) {
    drawTimeScreen();
  } else if (screenState == 1) {
    drawWeatherScreen();
  } else if (screenState == 2) {
    drawF1Screen();
  }
  
  display.display();
  delay(100); 
}

// --- Helper Functions ---

void handleButton() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    delay(50); 
    if (digitalRead(BUTTON_PIN) == LOW) {
      screenState++; 
      if(screenState > 2) screenState = 0; // Loop back to 0
    }
  }
  lastButtonState = currentButtonState;
}

void drawTimeScreen() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    display.setCursor(0, 20);
    display.println("Syncing Time...");
    return;
  }

  char timeStr[10];
  strftime(timeStr, sizeof(timeStr), "%I:%M:%S", &timeinfo); 
  
  char ampmStr[4];
  strftime(ampmStr, sizeof(ampmStr), "%p", &timeinfo); 

  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y", &timeinfo);

  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(timeStr);

  display.setTextSize(1);
  display.setCursor(100, 15);
  display.print(ampmStr);

  display.setTextSize(1);
  display.setCursor(15, 45);
  display.println(dateStr);
}

void drawWeatherScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Current Weather");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(currentTemp, 1);
  display.print(" C"); 

  const unsigned char* currentIcon = icon_sun; 
  if (currentWeatherCode >= 1 && currentWeatherCode <= 48) {
    currentIcon = icon_cloud;
  } else if (currentWeatherCode >= 51 && currentWeatherCode <= 99) {
    currentIcon = icon_rain;
  }
  
  display.drawBitmap(100, 18, currentIcon, 16, 16, WHITE);

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print("Humidity: ");
  display.print(currentHumidity);
  display.println("%");
  
  display.setCursor(0, 54);
  display.println(weatherStatus);
}

void drawF1Screen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("F1 Race Control");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 15);
  display.print("Flag: ");
  display.println(f1Flag);

  // Adafruit GFX automatically word-wraps long text at the screen edge
  display.setCursor(0, 30);
  display.println(f1Message);
}

void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherUrl);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        currentTemp = doc["current"]["temperature_2m"];
        currentHumidity = doc["current"]["relative_humidity_2m"];
        currentWeatherCode = doc["current"]["weather_code"]; 
        weatherStatus = getWeatherDescription(currentWeatherCode);
      }
    }
    http.end();
  }
}

void fetchF1Alerts() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(f1Url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      
      // Memory-efficient string parsing: find the last JSON object in the array
      int lastObjIndex = payload.lastIndexOf("{");
      if (lastObjIndex > 0) {
        
        // Extract the message
        int msgIndex = payload.indexOf("\"message\":\"", lastObjIndex);
        if (msgIndex > 0) {
          msgIndex += 11; // Move cursor past the string "message":"
          int endQuote = payload.indexOf("\"", msgIndex);
          f1Message = payload.substring(msgIndex, endQuote);
        }

        // Extract the flag (some messages don't have a flag, so we check)
        int flagIndex = payload.indexOf("\"flag\":\"", lastObjIndex);
        if (flagIndex > 0) {
          flagIndex += 8; // Move cursor past the string "flag":"
          int endQuote = payload.indexOf("\"", flagIndex);
          f1Flag = payload.substring(flagIndex, endQuote);
        } else {
          f1Flag = "NONE"; 
        }
      }
    }
    http.end();
  }
}

String getWeatherDescription(int code) {
  switch (code) {
    case 0: return "Clear Sky";
    case 1: case 2: case 3: return "Cloudy";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzle";
    case 61: case 63: case 65: return "Rain";
    case 71: case 73: case 75: return "Snow";
    case 95: case 96: case 99: return "Stormy";
    default: return "Unknown";
  }
}
