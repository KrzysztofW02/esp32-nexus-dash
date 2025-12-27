#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h> 
#include "secrets.h" 

// WIFI DATA 
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// SYSTEM COLORS
#define C_BG        0x0000 
#define C_ACCENT    0x07FF 
#define C_TEXT      0xFFFF 
#define C_GRAY      0x4208 
#define C_GREEN     0x07E0 
#define C_RED       0xF800 
#define C_BAR_BG    0x1021 

// ASSET COLORS
#define C_BTC_YELLOW 0xF7E0 
#define C_ETH_PURPLE 0x981F 

// PINS 
#define TFT_DE    5
#define TFT_VSYNC 3
#define TFT_HSYNC 46
#define TFT_PCLK  7
#define TFT_R0 1
#define TFT_R1 2
#define TFT_R2 42
#define TFT_R3 41
#define TFT_R4 40
#define TFT_G0 39
#define TFT_G1 0
#define TFT_G2 45
#define TFT_G3 48
#define TFT_G4 47
#define TFT_G5 21
#define TFT_B0 14
#define TFT_B1 38
#define TFT_B2 18
#define TFT_B3 17
#define TFT_B4 10
#define I2C_SDA 8
#define I2C_SCL 9

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    TFT_DE, TFT_VSYNC, TFT_HSYNC, TFT_PCLK,
    TFT_R0, TFT_R1, TFT_R2, TFT_R3, TFT_R4,
    TFT_G0, TFT_G1, TFT_G2, TFT_G3, TFT_G4, TFT_G5,
    TFT_B0, TFT_B1, TFT_B2, TFT_B3, TFT_B4,
    0, 20, 10, 20, 0, 10, 10, 10, 1, 8000000
);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(1024, 600, rgbpanel);

// Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); 

WebServer server(80);

// FINANCIAL VARIABLES
float kursBTC = 0.0; float staryKursBTC = 0.0; float zmiana24BTC = 0.0; int dirBTC = 0;           
float kursETH = 0.0; float staryKursETH = 0.0; float zmiana24ETH = 0.0; int dirETH = 0;
float kursUSD = 0.0; float staryKursUSD = 0.0; float zmiana24USD = 0.0; int dirUSD = 0;
float kursEUR = 0.0; float staryKursEUR = 0.0; float zmiana24EUR = 0.0; int dirEUR = 0;

// HISTORY 
const int GRAPH_POINTS = 40; 
float historyBTC[GRAPH_POINTS];
float historyETH[GRAPH_POINTS];

unsigned long lastUpdateData = 0; 
unsigned long lastClockUpdate = 0;

// HELPER FUNCTIONS

void updateHistory(float* history, float newPrice) {
  if (newPrice < 0.1 || isnan(newPrice)) return;
  for (int i = 0; i < GRAPH_POINTS - 1; i++) history[i] = history[i+1];
  if (history[0] == 0) for(int i=0; i<GRAPH_POINTS; i++) history[i] = newPrice;
  else history[GRAPH_POINTS - 1] = newPrice;
}

// DRAWING

void drawSeparator(int y) {
  gfx->fillRect(20, y, 984, 2, C_GRAY);
}

void drawTrendArrow(int x, int y, int size, int direction) {
  if (direction == 1) gfx->fillTriangle(x, y + size, x + (size/2), y, x + size, y + size, C_GREEN);
  else if (direction == -1) gfx->fillTriangle(x, y, x + (size/2), y + size, x + size, y, C_RED);
  else gfx->fillCircle(x + (size/2), y + (size/2), size/4, C_GRAY);
}

void drawCenteredPercentage(int centerX, int y, float percent) {
  gfx->setTextSize(3); 
  String text = "";
  if (percent > 0) text = "24h: +" + String(percent) + "%";
  else text = "24h: " + String(percent) + "%";
  
  int textWidth = text.length() * 18;
  int cursorX = centerX - (textWidth / 2);

  if (percent > 0) gfx->setTextColor(C_GREEN);
  else if (percent < 0) gfx->setTextColor(C_RED);
  else gfx->setTextColor(C_GRAY);

  gfx->setCursor(cursorX, y);
  gfx->print(text);
}

void drawMountainChart(int x, int y, int w, int h, float* data, uint16_t color) {
  float minVal = data[0];
  float maxVal = data[0];
  
  for(int i=1; i<GRAPH_POINTS; i++) {
    if(data[i] < minVal) minVal = data[i];
    if(data[i] > maxVal) maxVal = data[i];
  }

  float range = maxVal - minVal;
  if (range < 0.0001) { 
      gfx->drawFastHLine(x, y + h/2, w, C_GRAY); 
      return;
  }

  int step = w / GRAPH_POINTS; 

  for (int i = 0; i < GRAPH_POINTS; i++) {
    float normalized = (data[i] - minVal) / range;
    int barHeight = (int)(normalized * h);
    if (barHeight < 2) barHeight = 2;
    if (barHeight > h) barHeight = h;

    int xPos = x + (i * step);
    int yPos = y + h - barHeight;
    gfx->fillRect(xPos, yPos, step - 1, barHeight, color);
  }
}

// TOP BAR
void drawTopBarStatic() {
   gfx->fillRect(0, 0, 1024, 50, C_BAR_BG); 
   gfx->drawRect(0, 50, 1024, 2, C_ACCENT); 
   
   gfx->setTextSize(2);
   gfx->setTextColor(C_ACCENT); 
   gfx->setCursor(20, 15);
   gfx->print("SYSTEM: ONLINE");
   
   gfx->setTextColor(C_TEXT);
   gfx->setCursor(800, 15);
   gfx->print("IP: ");
   gfx->print(WiFi.localIP());
}

void updateTopClock() {
  timeClient.update();
  String timeStr = timeClient.getFormattedTime();
  gfx->fillRect(400, 5, 224, 40, C_BAR_BG); 
  gfx->setTextSize(3);
  int textWidth = timeStr.length() * 18; 
  int cursorX = 512 - (textWidth / 2);
  gfx->setTextColor(C_TEXT); 
  gfx->setCursor(cursorX, 15); 
  gfx->print(timeStr);
}

void drawCryptoSection() {
  gfx->fillRect(0, 55, 1024, 300, C_BG);
  gfx->fillRect(511, 70, 2, 270, C_GRAY);

  // BTC
  int btcX = 30; int btcCenterAxis = 405; 
  
  gfx->setTextSize(3);
  gfx->setTextColor(C_ACCENT); 
  gfx->setCursor(btcX, 80); gfx->print("BITCOIN");
  
  gfx->setTextSize(6);
  gfx->setTextColor(C_TEXT);   
  gfx->setCursor(btcX, 140); gfx->print(kursBTC, 1); 
  gfx->setTextSize(2); gfx->print(" USD");
  
  drawTrendArrow(btcCenterAxis - 25, 140, 50, dirBTC);
  drawCenteredPercentage(btcCenterAxis, 220, zmiana24BTC);
  
  drawMountainChart(btcX, 270, 450, 70, historyBTC, C_ACCENT);

  // ETH 
  int ethX = 540; int ethCenterAxis = 915;
  
  gfx->setTextSize(3);
  gfx->setTextColor(C_ACCENT); 
  gfx->setCursor(ethX, 80); gfx->print("ETHEREUM");
  
  gfx->setTextSize(6);
  gfx->setTextColor(C_TEXT);   
  gfx->setCursor(ethX, 140); gfx->print(kursETH, 1); 
  gfx->setTextSize(2); gfx->print(" USD");
  
  drawTrendArrow(ethCenterAxis - 25, 140, 50, dirETH);
  drawCenteredPercentage(ethCenterAxis, 220, zmiana24ETH);
  
  drawMountainChart(ethX, 270, 450, 70, historyETH, C_ACCENT);
}

void drawFiatSection() {
  gfx->fillRect(0, 360, 1024, 240, C_BG);
  drawSeparator(360);
  gfx->fillRect(511, 380, 2, 140, C_GRAY);

  // LEFT: EURO
  int eurX = 50; 
  int eurCenterAxis = 350;

  gfx->setTextSize(3);
  gfx->setTextColor(C_ACCENT);
  gfx->setCursor(eurX, 400);
  gfx->print("EUR / PLN");
  
  gfx->setTextSize(5);
  gfx->setTextColor(C_TEXT); 
  gfx->setCursor(eurX, 450);
  gfx->print(kursEUR, 4);

  drawTrendArrow(eurCenterAxis, 450, 40, dirEUR);
  drawCenteredPercentage(eurCenterAxis + 20, 530, zmiana24EUR);


  // RIGHT: USD
  int usdX = 550;
  int usdCenterAxis = 850;

  gfx->setTextSize(3);
  gfx->setTextColor(C_ACCENT);
  gfx->setCursor(usdX, 400);
  gfx->print("USD / PLN");
  
  gfx->setTextSize(5);
  gfx->setTextColor(C_TEXT);
  gfx->setCursor(usdX, 450);
  gfx->print(kursUSD, 4);

  drawTrendArrow(usdCenterAxis, 450, 40, dirUSD);
  drawCenteredPercentage(usdCenterAxis + 20, 530, zmiana24USD);
}

// NETWORK LOGIC
void fetchAllData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;
    
    // 1. BTC
    if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT")) {
      if (http.GET() == 200) {
        String payload = http.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        float nowy = doc["lastPrice"].as<float>();
        zmiana24BTC = doc["priceChangePercent"].as<float>();
        if (staryKursBTC != 0) { if (nowy > staryKursBTC) dirBTC = 1; else if (nowy < staryKursBTC) dirBTC = -1; }
        staryKursBTC = nowy; kursBTC = nowy;
        updateHistory(historyBTC, kursBTC);
      }
      http.end();
    }

    // 2. ETH
    if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=ETHUSDT")) {
      if (http.GET() == 200) {
        String payload = http.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        float nowy = doc["lastPrice"].as<float>();
        zmiana24ETH = doc["priceChangePercent"].as<float>();
        if (staryKursETH != 0) { if (nowy > staryKursETH) dirETH = 1; else if (nowy < staryKursETH) dirETH = -1; }
        staryKursETH = nowy; kursETH = nowy;
        updateHistory(historyETH, kursETH);
      }
      http.end();
    }

    // 3. USD
    float usdtPlnPrice = 0.0;
    float usdtPlnChange = 0.0;
    if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=USDTPLN")) {
      if (http.GET() == 200) {
        String payload = http.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        float nowy = doc["lastPrice"].as<float>();
        usdtPlnPrice = nowy;
        usdtPlnChange = doc["priceChangePercent"].as<float>();
        zmiana24USD = usdtPlnChange;
        if (staryKursUSD != 0) { if (nowy > staryKursUSD) dirUSD = 1; else if (nowy < staryKursUSD) dirUSD = -1; }
        staryKursUSD = nowy; kursUSD = nowy;
      }
      http.end();
    }

    // 4. EUR
    if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=EURUSDT")) {
      if (http.GET() == 200) {
        String payload = http.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        float eurUsdPrice = doc["lastPrice"].as<float>();
        float eurUsdChange = doc["priceChangePercent"].as<float>();
        float calculatedEurPln = eurUsdPrice * usdtPlnPrice;
        float calculatedChange = eurUsdChange + usdtPlnChange;
        zmiana24EUR = calculatedChange;
        if (staryKursEUR != 0) { if (calculatedEurPln > staryKursEUR) dirEUR = 1; else if (calculatedEurPln < staryKursEUR) dirEUR = -1; }
        staryKursEUR = calculatedEurPln; kursEUR = calculatedEurPln;
      }
      http.end();
    }
    
    drawCryptoSection();
    drawFiatSection();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  for(int i=0; i<GRAPH_POINTS; i++) { historyBTC[i]=0; historyETH[i]=0; }

  for(byte addr = 0x20; addr <= 0x27; addr++) {
       Wire.beginTransmission(addr);
       if (Wire.endTransmission() == 0) {
           Wire.beginTransmission(addr);
           Wire.write(0x01); Wire.write(0x01);
           Wire.endTransmission();
           break;
       }
   }

  gfx->begin();
  gfx->fillScreen(C_BG);

  gfx->setTextSize(3);
  gfx->setTextColor(C_ACCENT);
  gfx->setCursor(350, 300);
  gfx->print("SYSTEM BOOT...");
   
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      gfx->print(".");
  }

  timeClient.begin();
  timeClient.update();

  gfx->fillScreen(C_BG);
  drawTopBarStatic();
  updateTopClock();
  fetchAllData();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastClockUpdate >= 1000) {
    lastClockUpdate = currentMillis;
    updateTopClock();
  }

  if (currentMillis - lastUpdateData >= 15000) { 
    lastUpdateData = currentMillis;
    fetchAllData();
  }
}