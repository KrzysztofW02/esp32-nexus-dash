#include <U8g2lib.h>
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
#define C_BLACK     0x0000
#define C_DARK_GREEN 0x03E0 

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

// FINANCIAL VARIABLES
volatile float courseBTC = 0.0; float oldBTC = 0.0; volatile float change24BTC = 0.0; volatile float change1MBTC = 0.0; volatile float liveChangeBTC = 0.0; volatile int dirBTC = 0;          
volatile float courseETH = 0.0; float oldETH = 0.0; volatile float change24ETH = 0.0; volatile float change1METH = 0.0; volatile float liveChangeETH = 0.0; volatile int dirETH = 0;
volatile float courseUSD = 0.0; float oldUSD = 0.0; volatile float change24USD = 0.0; volatile float change1MUSD = 0.0; volatile float liveChangeUSD = 0.0; volatile int dirUSD = 0;
volatile float courseEUR = 0.0; float oldEUR = 0.0; volatile float change24EUR = 0.0; volatile float change1MEUR = 0.0; volatile float liveChangeEUR = 0.0; volatile int dirEUR = 0;

bool dataReadyToDraw = false; 

// HISTORY 
const int GRAPH_POINTS = 40; 
float historyBTC[GRAPH_POINTS];
float historyETH[GRAPH_POINTS];

unsigned long lastClockUpdate = 0;

void updateHistory(float* history, float newPrice) {
  if (newPrice < 0.00001 || isnan(newPrice)) return;
  
  for (int i = 0; i < GRAPH_POINTS - 1; i++) history[i] = history[i+1];
  
  history[GRAPH_POINTS - 1] = newPrice;
}

float getMonthlyOpen(const char* symbol) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  float openPrice = 0.0;
  http.setTimeout(3000); 
  String url = "https://api.binance.com/api/v3/klines?symbol=" + String(symbol) + "&interval=1M&limit=1";
  if (http.begin(client, url)) {
    if (http.GET() == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) openPrice = doc[0][1].as<float>();
    }
    http.end();
  }
  return openPrice;
}

// DRAWING HELPERS
void cleanRect(int x, int y, int w, int h) {
    gfx->fillRect(x, y, w, h, C_BG);
}

void drawSeparator(int y) {
  gfx->fillRect(20, y, 984, 2, C_GRAY);
}

void drawTrendArrow(int x, int y, int size, int direction) {
  cleanRect(x - 5, y - 5, size + 10, size + 10);
  if (direction == 1) gfx->fillTriangle(x, y + size, x + (size/2), y, x + size, y + size, C_GREEN);
  else if (direction == -1) gfx->fillTriangle(x, y, x + (size/2), y + size, x + size, y, C_RED);
  else gfx->fillCircle(x + (size/2), y + (size/2), size/4, C_GRAY);
}

void drawPercentLine(int x, int y, String label, float percent) {
  gfx->setFont(u8g2_font_helvB18_tr); 
  gfx->setTextSize(1);
  cleanRect(x, y - 20, 250, 25); 

  String text = label + ": " + String(percent, 2) + "%";
  if (percent > 0) text = label + ": +" + String(percent, 2) + "%";
  
  if (percent > 0) gfx->setTextColor(C_GREEN);
  else if (percent < 0) gfx->setTextColor(C_RED);
  else gfx->setTextColor(C_GRAY);

  gfx->setCursor(x, y);
  gfx->print(text);
  gfx->setFont((const GFXfont*)nullptr);
}

void drawLiveChange(int centerX, int y, float diffPercent) {
  gfx->setFont(u8g2_font_helvB18_tr); 
  gfx->setTextSize(1);
  cleanRect(centerX - 70, y - 25, 140, 30);

  String text = "";
  if (diffPercent > 0) text = "+" + String(diffPercent, 3) + "%";
  else if (diffPercent < 0) text = String(diffPercent, 3) + "%";
  else text = "0.000%"; 

  int textWidth = text.length() * 11; 
  int cursorX = centerX - (textWidth / 2);

  if (diffPercent > 0) gfx->setTextColor(C_GREEN);
  else if (diffPercent < 0) gfx->setTextColor(C_RED);
  else gfx->setTextColor(C_GRAY);

  gfx->setCursor(cursorX, y);
  gfx->print(text);
  gfx->setFont((const GFXfont*)nullptr);
}

void drawPrice(int x, int y, float price, int precision) {
    gfx->setFont(u8g2_font_logisoso46_tn);  
    gfx->setTextSize(1);
    gfx->setTextColor(C_TEXT); 
    cleanRect(x, y - 46, 300, 50);

    gfx->setCursor(x, y);
    gfx->print(price, precision);
    gfx->setFont((const GFXfont*)nullptr);
}
void drawMountainChart(int x, int y, int w, int h, float* data, uint16_t color) {
  cleanRect(x, y, w, h);

  float minVal = 99999999.0;
  float maxVal = -99999999.0;
  bool hasData = false;

  for(int i=0; i<GRAPH_POINTS; i++) {
    if(data[i] > 0.0001) {
       if(data[i] < minVal) minVal = data[i];
       if(data[i] > maxVal) maxVal = data[i];
       hasData = true;
    }
  }
  
  if (!hasData) return; 

  float range = maxVal - minVal;
  if (range < 0.0001) range = 1.0; 

  int step = w / GRAPH_POINTS; 
  
  for (int i = 0; i < GRAPH_POINTS; i++) {
    if (data[i] > 0.0001) {
        float normalized = (data[i] - minVal) / range;
        
        if (maxVal - minVal < 0.0001) normalized = 0.5;

        int barHeight = (int)(normalized * h);
        if (barHeight < 2) barHeight = 2;
        if (barHeight > h) barHeight = h;
        
        int xPos = x + (i * step);
        int yPos = y + h - barHeight;
        gfx->fillRect(xPos, yPos, step - 1, barHeight, color);
    }
  }
}

// ICONS
void drawBTCLogo(int x, int y) {
  gfx->fillCircle(x + 16, y + 16, 16, C_BTC_YELLOW);
  gfx->setTextSize(3);
  gfx->setTextColor(C_BLACK); 
  gfx->setCursor(x + 10, y + 6);
  gfx->print("B");
}

void drawETHLogo(int x, int y) {
  gfx->fillTriangle(x + 16, y, x + 6, y + 18, x + 26, y + 18, C_ETH_PURPLE);
  gfx->fillTriangle(x + 16, y + 32, x + 6, y + 20, x + 26, y + 20, C_ETH_PURPLE);
}

void drawEuroLogo(int x, int y) {
  gfx->fillCircle(x + 18, y + 18, 18, C_BTC_YELLOW);
  gfx->fillCircle(x + 22, y + 18, 14, C_BG);
  gfx->fillRect(x + 22, y, 20, 36, C_BG);
  gfx->fillRect(x + 2, y + 12, 24, 4, C_BTC_YELLOW);
  gfx->fillRect(x + 2, y + 20, 24, 4, C_BTC_YELLOW);
}

// TOP BAR
void drawTopBarStatic() {
   gfx->fillRect(0, 0, 1024, 50, C_BAR_BG); 
   gfx->drawRect(0, 50, 1024, 2, C_ACCENT); 
   gfx->setFont(u8g2_font_helvB14_tr); 
   gfx->setTextSize(1);
   gfx->setTextColor(C_ACCENT); 
   gfx->setCursor(20, 35);
   gfx->print("SYSTEM: ONLINE");
   gfx->setTextColor(C_TEXT);
   String ipStr = "IP: " + WiFi.localIP().toString();
   int ipWidth = ipStr.length() * 9;  
   gfx->setCursor(1024 - ipWidth - 15, 35);  
   gfx->print(ipStr);
   gfx->setFont((const GFXfont*)nullptr);
}

void updateTopClock() {
  timeClient.update();
  String timeStr = timeClient.getFormattedTime();
  gfx->fillRect(440, 0, 160, 48, C_BAR_BG);
  gfx->setFont(u8g2_font_profont29_mn);
  gfx->setTextSize(1);
  gfx->setTextColor(C_TEXT); 
  gfx->setCursor(444, 38);
  gfx->print(timeStr);
  gfx->setFont((const GFXfont*)nullptr);
}

void drawStaticInterface() {
    gfx->fillRect(0, 55, 1024, 300, C_BG);
    gfx->fillRect(511, 70, 2, 270, C_GRAY);
    gfx->fillRect(0, 360, 1024, 240, C_BG);
    drawSeparator(360);
    gfx->fillRect(511, 380, 2, 140, C_GRAY);

    int btcX = 30; int ethX = 540;
    int eurX = 50; int usdX = 550;

    // BTC 
    drawBTCLogo(btcX, 70);
    gfx->setFont(u8g2_font_helvB18_tr); gfx->setTextSize(1); gfx->setTextColor(C_ACCENT); 
    gfx->setCursor(btcX + 45, 100); gfx->print("BITCOIN");
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setCursor(btcX + 240, 180); gfx->print(" USD");

    // ETH 
    drawETHLogo(ethX, 70);
    gfx->setFont(u8g2_font_helvB18_tr); gfx->setTextColor(C_ACCENT); 
    gfx->setCursor(ethX + 45, 100); gfx->print("ETHEREUM");
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setCursor(ethX + 240, 180); gfx->print(" USD");

    // EUR 
    drawEuroLogo(eurX, 390);
    gfx->setFont(u8g2_font_helvB18_tr); gfx->setTextColor(C_ACCENT);
    gfx->setCursor(eurX + 45, 420); gfx->print("EURO"); 
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setCursor(eurX + 220, 500); gfx->print(" PLN");

    // USD 
    gfx->setFont(u8g2_font_logisoso32_tr); gfx->setTextColor(C_DARK_GREEN); gfx->setCursor(usdX, 420); gfx->print("$");
    gfx->setFont(u8g2_font_helvB18_tr); gfx->setTextColor(C_ACCENT); gfx->setCursor(usdX + 35, 420); gfx->print("USD");
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setCursor(usdX + 220, 500); gfx->print(" PLN");
    
    gfx->setFont((const GFXfont*)nullptr);
}

// REFRESH DYNAMIC DATA
void refreshDynamicData() {
  int btcX = 30; int btcCenterAxis = 405; 
  int ethX = 540; int ethCenterAxis = 915;
  int eurX = 50; int eurCenterAxis = 405; 
  int usdX = 550; int usdCenterAxis = 915;

  // BTC
  drawPrice(btcX, 180, courseBTC, 1);
  drawPercentLine(btcX, 215, "24h", change24BTC);
  drawPercentLine(btcX, 255, "1 Month", change1MBTC);
  drawTrendArrow(btcCenterAxis - 25, 140, 50, dirBTC);
  drawLiveChange(btcCenterAxis, 220, liveChangeBTC);
  drawMountainChart(btcX, 275, 450, 60, historyBTC, C_ACCENT);

  // ETH
  drawPrice(ethX, 180, courseETH, 1);
  drawPercentLine(ethX, 215, "24h", change24ETH);
  drawPercentLine(ethX, 255, "1 Month", change1METH);
  drawTrendArrow(ethCenterAxis - 25, 140, 50, dirETH);
  drawLiveChange(ethCenterAxis, 220, liveChangeETH);
  drawMountainChart(ethX, 275, 450, 60, historyETH, C_ACCENT);

  // EUR
  drawPrice(eurX, 500, courseEUR, 4);
  drawPercentLine(eurX, 535, "24h", change24EUR);
  drawPercentLine(eurX, 575, "1 Month", change1MEUR);
  drawTrendArrow(eurCenterAxis - 25, 460, 50, dirEUR);
  drawLiveChange(eurCenterAxis, 540, liveChangeEUR);

  // USD
  drawPrice(usdX, 500, courseUSD, 4);
  drawPercentLine(usdX, 535, "24h", change24USD);
  drawPercentLine(usdX, 575, "1 Month", change1MUSD);
  drawTrendArrow(usdCenterAxis - 25, 460, 50, dirUSD);
  drawLiveChange(usdCenterAxis, 540, liveChangeUSD);
}

// NETWORK TASK
void NetworkTask(void * parameter) {
  int loopCounter = 0; 
  
  for(;;) { 
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure(); 
        HTTPClient http;
        http.setTimeout(4000); 

        bool update1M = (loopCounter == 0); 
        
        // 1. BTC
        if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT")) {
            if (http.GET() == 200) {
                String payload = http.getString();
                JsonDocument doc;
                deserializeJson(doc, payload);
                float nowy = doc["lastPrice"].as<float>();
                change24BTC = doc["priceChangePercent"].as<float>();
                if (update1M) {
                    float mOpen = getMonthlyOpen("BTCUSDT");
                    if(mOpen > 0) change1MBTC = ((nowy - mOpen) / mOpen) * 100.0;
                }
                if (oldBTC != 0) { 
                   if (nowy > oldBTC) dirBTC = 1; else if (nowy < oldBTC) dirBTC = -1; else dirBTC = 0;
                   liveChangeBTC = ((nowy - oldBTC) / oldBTC) * 100.0;
                }
                oldBTC = nowy; courseBTC = nowy;
                updateHistory(historyBTC, courseBTC);
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
                change24ETH = doc["priceChangePercent"].as<float>();
                if (update1M) {
                    float mOpen = getMonthlyOpen("ETHUSDT");
                    if(mOpen > 0) change1METH = ((nowy - mOpen) / mOpen) * 100.0;
                }
                if (oldETH != 0) { 
                  if (nowy > oldETH) dirETH = 1; else if (nowy < oldETH) dirETH = -1; else dirETH = 0;
                  liveChangeETH = ((nowy - oldETH) / oldETH) * 100.0;
                }
                oldETH = nowy; courseETH = nowy;
                updateHistory(historyETH, courseETH);
            }
            http.end();
        }
        
        float usdtPlnPrice = 0.0;
        float usdtPlnChange = 0.0;
        
        // USD
        if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=USDTPLN")) {
            if (http.GET() == 200) {
                String payload = http.getString();
                JsonDocument doc;
                deserializeJson(doc, payload);
                float nowy = doc["lastPrice"].as<float>();
                usdtPlnPrice = nowy;
                usdtPlnChange = doc["priceChangePercent"].as<float>();
                change24USD = usdtPlnChange;
                if (update1M) {
                    float mOpen = getMonthlyOpen("USDTPLN");
                    if(mOpen > 0) change1MUSD = ((nowy - mOpen) / mOpen) * 100.0;
                }
                if (oldUSD != 0) { 
                  if (nowy > oldUSD) dirUSD = 1; else if (nowy < oldUSD) dirUSD = -1; else dirUSD = 0;
                  liveChangeUSD = ((nowy - oldUSD) / oldUSD) * 100.0;
                }
                oldUSD = nowy; courseUSD = nowy;
            }
            http.end();
        }

        // EUR
        if (http.begin(client, "https://api.binance.com/api/v3/ticker/24hr?symbol=EURUSDT")) {
            if (http.GET() == 200) {
                String payload = http.getString();
                JsonDocument doc;
                deserializeJson(doc, payload);
                float eurUsdPrice = doc["lastPrice"].as<float>();
                float eurUsdChange = doc["priceChangePercent"].as<float>();
                
                float calculatedEurPln = eurUsdPrice * usdtPlnPrice;
                change24EUR = eurUsdChange + usdtPlnChange;
                
                if (update1M) {
                      float mOpenEurUsd = getMonthlyOpen("EURUSDT");
                      if(mOpenEurUsd > 0) {
                          float changeEurUsdMonth = ((eurUsdPrice - mOpenEurUsd) / mOpenEurUsd) * 100.0;
                          change1MEUR = changeEurUsdMonth + change1MUSD;
                      }
                }
                if (oldEUR != 0) { 
                  if (calculatedEurPln > oldEUR) dirEUR = 1; else if (calculatedEurPln < oldEUR) dirEUR = -1; else dirEUR = 0;
                  liveChangeEUR = ((calculatedEurPln - oldEUR) / oldEUR) * 100.0;
                }
                oldEUR = calculatedEurPln; courseEUR = calculatedEurPln;
            }
            http.end();
        }

        dataReadyToDraw = true; 
        
        loopCounter++;
        if(loopCounter >= 20) loopCounter = 0; 
    }
    vTaskDelay(15000 / portTICK_PERIOD_MS);
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
  drawStaticInterface();

  xTaskCreatePinnedToCore(
    NetworkTask,   
    "NetTask",     
    10000,         
    NULL,          
    1,             
    NULL,          
    0              
  );
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastClockUpdate >= 1000) {
    lastClockUpdate = currentMillis;
    updateTopClock();
  }

  if (dataReadyToDraw) {
     refreshDynamicData();
     dataReadyToDraw = false; 
  }
  
  delay(10);
}