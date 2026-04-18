#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- CREDENTIALS ---
#define WIFI_SSID        "IIITDM"
#define WIFI_PASSWORD    "pendrive"

#define FIREBASE_API_KEY       "AIzaSyCeQan2EK1OQVxhsDMsQ793mHdl4LxuMwI"
#define FIREBASE_DATABASE_URL  "https://aeris-monitor-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_USER_EMAIL    "aeris_hardware@test.com"
#define FIREBASE_USER_PASSWORD "AerisDevice99!"

// --- DISPLAY SETTINGS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- PIN DEFINITIONS ---
const int DUST_LED_PIN = 4;      // Sharp Dust Sensor LED
const int DUST_OUT_PIN = 34;     // Sharp Dust Sensor Analog Out
const int GAS_SENSOR_PIN = 35;   // MQ135 Analog Out
const int BUZZER_PIN = 18;       // 5V Buzzer

// --- VARIABLES ---
float dustDensity = 0;
int gasLevel = 0;
const int GAS_WARNING_LEVEL = 1500;    
const float DUST_WARNING_LEVEL = 0.15; 

// --- FIREBASE OBJECTS ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long lastPushTime = 0;

void setup() {
  Serial.begin(115200);

  // Initialize Pins
  pinMode(DUST_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off on boot

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("AERIS OS");
  display.setTextSize(1);
  display.setCursor(20, 45);
  display.println("Connecting WiFi...");
  display.display();

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  display.clearDisplay();
  display.setCursor(10, 30);
  display.println("WiFi Connected!");
  display.display();

  // Connect Firebase
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  auth.user.email = FIREBASE_USER_EMAIL;
  auth.user.password = FIREBASE_USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // 1. READ DUST SENSOR 
  digitalWrite(DUST_LED_PIN, LOW); 
  delayMicroseconds(280);          
  int dustRaw = analogRead(DUST_OUT_PIN); 
  delayMicroseconds(40);
  digitalWrite(DUST_LED_PIN, HIGH); 
  delayMicroseconds(9680);         

  float calcVoltage = dustRaw * (3.3 / 4095.0);
  dustDensity = 0.17 * calcVoltage - 0.1;
  if (dustDensity < 0) {
    dustDensity = 0.00; 
  }

  // 2. READ GAS SENSOR
  gasLevel = analogRead(GAS_SENSOR_PIN); 

  // 3. LOGIC & CONTROL 
  bool isPolluted = false;
  if (dustDensity > DUST_WARNING_LEVEL || gasLevel > GAS_WARNING_LEVEL) {
    isPolluted = true;
  } 

  // 4. BUZZER ALARM 
  if (isPolluted) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100); 
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 5. UPDATE OLED DISPLAY (v1.4 Logic)
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("AERIS Purifier");

  // *** THE FAKE MOTOR SPEED LINE ***
  display.setCursor(0, 14);
  display.print("Motor: 80% [AUTO]"); 

  display.setCursor(0, 28);
  display.print("Dust: ");
  display.print(dustDensity);
  display.print(" mg/m3");

  display.setCursor(0, 42);
  display.print("Gas Raw: ");
  display.print(gasLevel);

  display.setCursor(0, 56);
  display.print("Air: ");
  if (isPolluted) {
    display.print("POOR (Filtering)");
  } else {
    display.print("CLEAN");
  }
  display.display();

  // 6. FIREBASE PUSH
  if (Firebase.ready() && (millis() - lastPushTime > 2000)) {
    lastPushTime = millis();
    FirebaseJson payload;
    payload.set("dust_density", dustDensity);
    payload.set("mq135_raw", gasLevel);
    // Push a status string so the dashboards know the state
    payload.set("status", isPolluted ? "POOR" : "CLEAN");
    Firebase.RTDB.setJSON(&fbdo, "/aeris/live", &payload);
    Serial.println("Data pushed to Firebase");
  }

  delay(1500); 
}