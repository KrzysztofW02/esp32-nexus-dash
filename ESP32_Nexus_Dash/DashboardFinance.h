#ifndef DASHBOARDFINANCE_H
#define DASHBOARDFINANCE_H

#include <Arduino_GFX_Library.h>

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

extern Arduino_RGB_Display *gfx;
extern const int GRAPH_POINTS;
extern float historyBTC[];
extern float historyETH[];

// FINANCIAL VARIABLES
extern volatile float courseBTC, change24BTC, change1MBTC, liveChangeBTC; extern volatile int dirBTC;
extern volatile float courseETH, change24ETH, change1METH, liveChangeETH; extern volatile int dirETH;
extern volatile float courseUSD, change24USD, change1MUSD, liveChangeUSD; extern volatile int dirUSD;
extern volatile float courseEUR, change24EUR, change1MEUR, liveChangeEUR; extern volatile int dirEUR;

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

void drawStaticInterfaceFinance() {
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
void refreshDynamicDataFinance() {
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

#endif