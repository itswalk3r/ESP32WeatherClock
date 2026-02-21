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
#define NEXT_BTN_PIN 13    // Cycles through the 5 screens
#define REFRESH_BTN_PIN 14 // Forces an immediate API update

// --- WiFi Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- API Key Rotation Array ---
const int NUM_KEYS = 3;
const char* apiKeys[NUM_KEYS] = {
  "YOUR_KEY_1",
  "YOUR_KEY_2",
  "YOUR_KEY_3"
};
int currentKeyIndex = 0; 

// --- Time Configuration ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST (+5:30)
const int   daylightOffset_sec = 0;

// --- Global Variables & Timers ---
int screenState = 0; // 0=Clock, 1=Match, 2=Batters, 3=Points, 4=Schedule
bool lastNextBtnState = HIGH;
bool lastRefreshBtnState = HIGH;

unsigned long lastScreenInteraction = 0;
const unsigned long screenTimeout = 5000; // 5 seconds

unsigned long lastApiCall = 0;
const unsigned long apiUpdateInterval = 120000; // 2 minutes (120,000 ms)

// --- Cricket Data Variables (Placeholders) ---
// Screen 1: Match Status
String team1Name = "IND";
String team1Score = "110/8";
String team1Overs = "14.2";
String team2Name = "AUS";
String team2Score = "150/10";
String team2Overs = "20.0";
String matchStatus = "Innings Break";

// Screen 2: Batters
String batter1Name = "Kohli";
String batter1Stats = "50 (40)";
String batter2Name = "Rohit";
String batter2Stats = "32* (20)";

// Screen 3: Points Table (Top 4)
String ptTeams[4] = {"IND", "AUS", "ENG", "SA"};
String ptScores[4] = {"8", "6", "4", "4"};
String ptNRR[4] = {"+1.5", "+0.8", "-0.1", "-0.5"};

// Screen 4: Schedule (Next 3)
String nextMatches[3] = {
  "IND v PAK - 24 Feb",
  "AUS v NZ  - 25 Feb",
  "ENG v WI  - 26 Feb"
};

void setup() {
  Serial.begin(115200);
  pinMode(NEXT_BTN_PIN, INPUT_PULLUP);
  pinMode(REFRESH_BTN_PIN, INPUT_PULLUP);

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
  
  // Initial Fetch
  fetchCricketData();
}

void loop() {
  handleButtons();

  // Screen Auto-Timeout Logic (5 Seconds -> Clock)
  if (screenState != 0 && (millis() - lastScreenInteraction > screenTimeout)) {
    screenState = 0; 
  }

  // Background API Update (2 mins)
  if (millis() - lastApiCall > apiUpdateInterval) {
    fetchCricketData();
    lastApiCall = millis();
  }

  display.clearDisplay();
  
  if (screenState == 0) drawClockScreen();
  else if (screenState == 1) drawMatchScreen();
  else if (screenState == 2) drawBatterScreen();
  else if (screenState == 3) drawPointsScreen();
  else if (screenState == 4) drawScheduleScreen();
  
  display.display();
  delay(50); // Small loop delay for stability
}

// --- Button Handling ---
void handleButtons() {
  // Handle NEXT Button (Screen Toggle)
  bool currentNextState = digitalRead(NEXT_BTN_PIN);
  if (currentNextState == LOW && lastNextBtnState == HIGH) {
    delay(50); // Debounce
    if (digitalRead(NEXT_BTN_PIN) == LOW) {
      screenState++; 
      if(screenState > 4) screenState = 0; 
      lastScreenInteraction = millis(); // Reset screen timeout
    }
  }
  lastNextBtnState = currentNextState;

  // Handle REFRESH Button (Force API Call)
  bool currentRefreshState = digitalRead(REFRESH_BTN_PIN);
  if (currentRefreshState == LOW && lastRefreshBtnState == HIGH) {
    delay(50); 
    if (digitalRead(REFRESH_BTN_PIN) == LOW) {
      // Show loading feedback
      display.clearDisplay();
      display.setCursor(20, 30);
      display.println("Fetching Live Data...");
      display.display();
      
      fetchCricketData(); // Force call
      lastApiCall = millis(); // Reset the 2-minute timer
      lastScreenInteraction = millis(); // Keep screen awake
    }
  }
  lastRefreshBtnState = currentRefreshState;
}

// --- Screen Drawing Functions ---

void drawClockScreen() {
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
  display.setCursor(15, 45);
  display.println(dateStr);
}

void drawMatchScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(team1Name);
  display.print(" vs ");
  display.println(team2Name);
  display.drawLine(0, 10, 128, 10, WHITE);

  // Team 1
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(team1Name);
  display.print(" ");
  display.println(team1Score);
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Overs: ");
  display.println(team1Overs);

  // Team 2 (Smaller to fit)
  display.setCursor(0, 45);
  display.print(team2Name);
  display.print(": ");
  display.print(team2Score);
  display.print(" (");
  display.print(team2Overs);
  display.println(")");

  // Status
  display.setCursor(0, 56);
  display.println(matchStatus);
}

void drawBatterScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("At the Crease");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setTextSize(2);
  // Batter 1
  display.setCursor(0, 15);
  display.println(batter1Name);
  display.setTextSize(1);
  display.setCursor(80, 22);
  display.println(batter1Stats);

  display.setTextSize(2);
  // Batter 2
  display.setCursor(0, 40);
  display.println(batter2Name);
  display.setTextSize(1);
  display.setCursor(80, 47);
  display.println(batter2Stats);
}

void drawPointsScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Rank | Team | Pts | NRR");
  display.drawLine(0, 10, 128, 10, WHITE);

  for(int i = 0; i < 4; i++) {
    int y = 15 + (i * 12);
    display.setCursor(0, y);
    display.print(i+1);
    display.setCursor(30, y);
    display.print(ptTeams[i]);
    display.setCursor(70, y);
    display.print(ptScores[i]);
    display.setCursor(100, y);
    display.print(ptNRR[i]);
  }
}

void drawScheduleScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Upcoming Matches");
  display.drawLine(0, 10, 128, 10, WHITE);

  for(int i = 0; i < 3; i++) {
    int y = 15 + (i * 15);
    display.setCursor(0, y);
    display.println(nextMatches[i]);
  }
}

// --- API Fetching with Key Rotation ---
void fetchCricketData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Construct URL with the currently active key
    String url = "https://api.cricketdata.org/v1/cricScore?apikey=" + String(apiKeys[currentKeyIndex]);
    
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      Serial.println("Cricket data fetched successfully!");
      String payload = http.getString();
      
      // -- JSON PARSING BLOCK --
      // *Note: Adjust these paths to match your exact endpoint's JSON structure!*
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if(!error) {
        // Example logic mapping variables:
        // team1Name = doc["data"][0]["t1"].as<String>();
        // team1Score = doc["data"][0]["t1s"].as<String>();
        // matchStatus = doc["data"][0]["status"].as<String>();
      }

    } else if (httpCode == 429 || httpCode == 403) {
      // Error catch for limits reached
      Serial.print("Key ");
      Serial.print(currentKeyIndex + 1);
      Serial.println(" exhausted. Rotating...");
      
      currentKeyIndex++; // Bump to next key
      
      if (currentKeyIndex >= NUM_KEYS) {
        Serial.println("CRITICAL: All 3 API keys exhausted!");
        currentKeyIndex = 0; // Reset back to 0 (will likely fail until tomorrow)
      } else {
        http.end();
        fetchCricketData(); // Recursive call immediately tries the next key
        return; 
      }
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpCode);
    }
    http.end();
  }
}
