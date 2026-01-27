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
#include "DashboardFinance.h"
#include "DashboardPC.h" 
#include "DashboardCamera.h"
#include "DashboardCloudflare.h"
#include "WebInterface.h"

// GLOBAL VARIABLES
WebServer server(80);
int currentDashboard = 0; // 0 = Finance, 1 = PC Monitor, 2 = Camera, 3 = Cloudflare Analytics
bool needRedrawStatic = false; 

// WIFI DATA 
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

IPAddress local_IP(192, 168, 1, 47);
IPAddress gateway(192, 168, 1, 1);    
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);    
IPAddress secondaryDNS(8, 8, 4, 4);

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

// NETWORK TASK
void NetworkTask(void * parameter) {
  int loopCounter = 0; 
  
  for(;;) { 
    switch (currentDashboard) {
      // CASE 0: FINANCE DASHBOARD
        case 0:
            if (WiFi.status() == WL_CONNECTED) {
                WiFiClientSecure client;
                client.setInsecure(); 
                HTTPClient http;
                http.setTimeout(4000); 

                bool update1M = (loopCounter == 0); 
                
                // BTC
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

                // ETH
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
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            break;

        // CASE 1: PC MONITOR
        case 1:
            vTaskDelay(1000 / portTICK_PERIOD_MS); 
            break;
            
        // CASE 2: CAMERA
        case 2:
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
            
        default:
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
    }
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
  
  setupCameraDashboard();
  
  gfx->fillScreen(C_BG);

  gfx->setTextSize(3);
  gfx->setTextColor(C_ACCENT);
  gfx->setCursor(350, 300);
  gfx->print("SYSTEM BOOT...");
  
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      gfx->print(".");
  }
  
  WiFi.setSleep(false);

  // WEB SERVER START
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/update_pc", HTTP_POST, handleUpdatePC);
  server.on("/update_cloudflare", HTTP_POST, handleUpdateCloudflare);
  
  server.begin();
  Serial.println("Web server started");

  timeClient.begin();
  timeClient.update();

  gfx->fillScreen(C_BG);
  drawTopBarStatic();
  updateTopClock();
  
  if (currentDashboard == 0) {
    drawStaticInterfaceFinance();
  }

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
  server.handleClient();

  // DASHBOARD SWITCHING
  if (needRedrawStatic) {
      dataReadyToDraw = false; 
      
      gfx->fillScreen(C_BG);

      if (currentDashboard != 2 && currentDashboard != 3) {
          drawTopBarStatic(); 
          updateTopClock();
      }
      
      if (currentDashboard == 0) {
        drawStaticInterfaceFinance();
      } else if (currentDashboard == 1) {
        drawStaticInterfacePC(gfx);
      } else if (currentDashboard == 2) { 
        drawStaticInterfaceCamera();
      } else if (currentDashboard == 3) {
        drawStaticInterfaceCloudflare();
      }
      needRedrawStatic = false;
  }

  // CLOCK UPDATE
  if (currentDashboard != 2 && currentDashboard != 3) {
      if (currentMillis - lastClockUpdate >= 1000) {
        lastClockUpdate = currentMillis;
        updateTopClock();
      }
  }

  // DYNAMIC DATA REFRESH
  
  // Dashboard 0: Finance
  if (currentDashboard == 0 && dataReadyToDraw) {
      refreshDynamicDataFinance();
      dataReadyToDraw = false; 
  }
  
  // Dashboard 1: PC Monitor
  if (currentDashboard == 1 && dataReadyToDraw) {
      refreshDynamicDataPC(gfx);
      dataReadyToDraw = false; 
  }

  // Dashboard 2: Camera
  if (currentDashboard == 2) {
    refreshCameraFrame();
  } 
  // Dashboard 3: Cloudflare Security
  else if (currentDashboard == 3) {
    refreshDynamicDataCloudflare();
  } else {
    delay(10);
  }
}