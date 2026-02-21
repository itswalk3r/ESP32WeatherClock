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
const char* ssid = "SSID";
const char* password = "password";

// --- API & Time Configuration ---
// Open-Meteo URL (Current Temp, Humidity, and Weather Code)
const char* weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=9.9312&longitude=76.2673&current=temperature_2m,relative_humidity_2m,weather_code";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // 19800 seconds = 5.5 hours (IST)
const int   daylightOffset_sec = 0;

// --- Global Variables ---
int screenState = 0; // 0 = Time/Date, 1 = Weather
bool lastButtonState = HIGH;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 600000; // Update weather every 10 minutes

float currentTemp = 0.0;
int currentHumidity = 0;
String weatherStatus = "Fetching...";

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize OLED
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

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Initialize NTP Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Initial Weather Fetch
  fetchWeather();
}

void loop() {
  handleButton();

  // Update weather in the background every 10 minutes
  if (millis() - lastWeatherUpdate > weatherUpdateInterval) {
    fetchWeather();
    lastWeatherUpdate = millis();
  }

  display.clearDisplay();
  
  if (screenState == 0) {
    drawTimeScreen();
  } else {
    drawWeatherScreen();
  }
  
  display.display();
  delay(100); // Small delay to prevent display flickering
}

// --- Helper Functions ---

void handleButton() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  // Check if button was pressed (LOW) and debounced
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      screenState = !screenState; // Toggle between 0 and 1
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
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  
  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y", &timeinfo);

  display.setTextSize(2);
  display.setCursor(15, 15);
  display.println(timeStr);

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
  display.print(" C"); // Using standard C, circle drawing for degree omitted for simplicity

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print("Humidity: ");
  display.print(currentHumidity);
  display.println("%");
  
  display.setCursor(0, 54);
  display.println(weatherStatus);
}

void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherUrl);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      
      // Parse JSON
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        currentTemp = doc["current"]["temperature_2m"];
        currentHumidity = doc["current"]["relative_humidity_2m"];
        int code = doc["current"]["weather_code"];
        weatherStatus = getWeatherDescription(code);
      }
    }
    http.end();
  }
}

// Converts standard WMO codes to text
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
