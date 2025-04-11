#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <SPI.h>
#include <Fonts/FreeMono9pt7b.h>

// Pin assignments
const int trigPin = 2;
const int echoPin = 3;

#define LED_PIN    4
// Using hardware Serial (pins 0,1) for HM-10

const int distanceThreshold = 50;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI   11
#define OLED_CLK    13
#define OLED_CS     10
#define OLED_DC     9
#define OLED_RESET  8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);
U8G2_FOR_ADAFRUIT_GFX u8g2;

bool bleConnected = false;
bool recievedDisplay = false;
bool wasDetected = false;
String lastTime = "";
String lastWeather = "";
int lastBatteryLevel = 80;

void setup() {
  // Initialize hardware Serial for HM-10
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    while (true);  // Don't proceed if OLED fails
  }

  u8g2.begin(display); 

  display.clearDisplay();
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(WHITE);

  // First screen
  u8g2.setFont(u8g2_font_helvR14_tf);
  u8g2.setCursor(15, 35);
  u8g2.print(F("Motovision"));

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(10, 50);
  u8g2.print(F("Waiting for BT..."));

  display.display();
}

void drawInvertedSpeedLimitSign(int x, int y, int speed) {
  int width = 40;
  int height = 50;
  int cornerRadius = 3;

  // 1. Fill rounded rectangle
  display.fillRoundRect(x, y, width, height, cornerRadius, SSD1306_WHITE);

  // 2. Draw border
  display.drawRoundRect(x, y, width, height, cornerRadius, SSD1306_BLACK);

  // 3. Black text over white background
  display.setTextColor(SSD1306_BLACK);

  // "SPEED"
  display.setTextSize(1);
  int speedLabelX = x + (width - 6 * 5) / 2;
  display.setCursor(speedLabelX, y + 6);
  display.print("SPEED");

  // "LIMIT"
  int limitLabelX = x + (width - 6 * 5) / 2;
  display.setCursor(limitLabelX, y + 14);
  display.print("LIMIT");

  // Digits
  display.setTextSize(2);  // smaller size for digits to fit in compact sign
  String speedStr = String(speed);
  int textWidth = speedStr.length() * 6;  // 6px per char at size 1
  int speedX = x + (width - 24) / 2;  // 2 chars * 12px ≈ 24px

  display.setCursor(speedX, y + 30);
  display.print(speedStr);

  // Reset styles
  display.setTextColor(SSD1306_WHITE);
}


void handleData(String command) {
  // Reset all values first
  lastTime = "";
  lastWeather = "";

  // Split the string on ';'
  int start = 0;
  while (start < command.length()) {
      int end = command.indexOf(';', start);
      if (end == -1) end = command.length();

      String token = command.substring(start, end);
      if (token.startsWith("T:")) {
          lastTime = token.substring(2); // Strip off 'T:'
      } else if (token.startsWith("W:")) {
          lastWeather = token.substring(2); // Strip off 'W:'
      }

      start = end + 1;
  }

  handleDisplayUpdate();
}

void handleDisplayUpdate(){
  display.clearDisplay();
      
  u8g2.setFont(u8g2_font_6x10_tf);
    
  // Position each element with proper spacing
  int yPos = 20;
  const int lineHeight = 10;  // Height for each line of text

  if (lastTime.length() > 0) {
      u8g2.setCursor(0, yPos);
      u8g2.print("TIME: ");
      u8g2.print(lastTime);
      yPos += lineHeight;
  }
  
  if (lastWeather.length() > 0) {
      u8g2.setCursor(0, yPos);
      u8g2.print("WEATHER: ");
      u8g2.print(lastWeather);
      yPos += lineHeight;
  }

  u8g2.setCursor(0, yPos);
  u8g2.println("MOTOVISION HUD");
  yPos += lineHeight;

  // Battery display
  u8g2.setCursor(0, yPos);
  u8g2.print("BATTERY: ");
  
  // Battery Bar - moved down to align with text
  int barWidth = map(lastBatteryLevel, 0, 100, 0, 30);  // Smaller bar width
  int barX = 50;  // Position after "BATTERY:" text
  display.drawRect(barX, yPos - 6, 32, 6, SSD1306_WHITE);  // Smaller battery outline
  display.fillRect(barX + 1, yPos - 5, barWidth, 4, SSD1306_WHITE);  // Smaller battery fill
  u8g2.println("");
  yPos += lineHeight;

  u8g2.setCursor(0, yPos);
  u8g2.print("Blindspot: ");
  u8g2.println(wasDetected ? "ALERT" : "CLEAR");
  display.display();
}

bool isBlindspotDetected() {
  long duration, distanceCM;

  // Trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo duration
  duration = pulseIn(echoPin, HIGH);
  distanceCM = (duration / 2) / 29.1;

  // Serial.print("Measured Distance: ");
  // Serial.print(distanceCM);
  // Serial.println(" cm");
  
  return (distanceCM <= distanceThreshold && distanceCM > 0);
}

void loop() {

  static unsigned long previousMillis = 0;
  const unsigned long interval = 1000; // Check every second
  unsigned long currentMillis = millis();
  if (bleConnected && currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if(recievedDisplay && wasDetected != isBlindspotDetected()){
      wasDetected = !wasDetected;
      Serial.println(wasDetected);
      handleDisplayUpdate();
    }

  }

  if (!bleConnected && Serial.available()) {
    char c = Serial.read();

    if (c == 'c') {
      bleConnected = true;
      Serial.println("connected");

      display.clearDisplay();
      u8g2.setFont(u8g2_font_helvR14_tf);
      u8g2.setCursor(15, 35);
      u8g2.print("Connected!");
      display.display();
      delay(2000);
    }
    return;
  }

  if (bleConnected && Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();  // Clean up extra characters

    if (command == "disconnect") {
      bleConnected = false;
      
      display.clearDisplay();
      u8g2.setFont(u8g2_font_helvR14_tf);
      u8g2.setCursor(10, 32);
      u8g2.print("Disconnected!");
      display.display();
      delay(2000);
      
      display.clearDisplay();
      u8g2.setFont(u8g2_font_helvR14_tf);
      u8g2.setCursor(15, 35);
      u8g2.print(F("Motovision"));

      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(10, 50);
      u8g2.print(F("Waiting for BT..."));
      display.display();
    } else if (command.startsWith("data")) {
      recievedDisplay = true;
      String payload = command.substring(4);
      handleData(payload);
    }
  }
  delay(100);
  
}