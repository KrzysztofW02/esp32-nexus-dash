#ifndef DASHBOARDPC_H
#define DASHBOARDPC_H

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>

// Colors
#define C_BG            0x0000 
#define C_HUD_CYAN      0x07FF 
#define C_HUD_MAGENTA   0xF81F 
#define C_HUD_YELLOW    0xFFE0 
#define C_HUD_RED       0xF800 
#define C_HUD_DARK      0x18E3 
#define C_HUD_GRID      0x0861 

struct PCStats {
  int cpuLoad = 0;
  int cpuTemp = 0;
  int gpuLoad = 0;
  int gpuTemp = 0;
  int ramUsed = 0;
  int ramTotal = 32; 
  String netUp = "0.0";
  String netDown = "0.0";
  int ping = 0;
};

PCStats pcData;

// Brackets
void drawSciFiBracket(Arduino_RGB_Display *gfx, int x, int y, int w, int h, uint16_t color, String label) {
    int len = 30; // Długość rogów
    int thick = 3; // Grubość rogów

    // Left Top
    gfx->fillRect(x, y, len, thick, color);
    gfx->fillRect(x, y, thick, len, color);

    // Right Top
    gfx->fillRect(x + w - len, y, len, thick, color);
    gfx->fillRect(x + w - thick, y, thick, len, color);

    // Left Bottom
    gfx->fillRect(x, y + h - thick, len, thick, color);
    gfx->fillRect(x, y + h - len, thick, len, color);

    // Right Bottom
    gfx->fillRect(x + w - len, y + h - thick, len, thick, color);
    gfx->fillRect(x + w - thick, y + h - len, thick, len, color);

    gfx->drawFastHLine(x + len + 10, y + 1, w - (2*len) - 20, C_HUD_DARK);
    gfx->drawFastHLine(x + len + 10, y + h - 1, w - (2*len) - 20, C_HUD_DARK);

    // Label
    if (label != "") {
        int labelWidth = (label.length() * 16) + 20;
        
        gfx->fillRect(x + 10, y - 12, labelWidth, 24, C_BG);
        gfx->drawRect(x + 10, y - 12, labelWidth, 24, color);
        
        gfx->setFont(u8g2_font_helvB14_tr);
        gfx->setTextColor(color);
        gfx->setCursor(x + 20, y + 6);
        gfx->print(label);
    }
}

// Progress bar
void drawTechBar(Arduino_RGB_Display *gfx, int x, int y, int w, int h, int val, int maxVal, uint16_t activeColor) {
    if (maxVal <= 0) maxVal = 1;
    
    gfx->fillRect(x, y, w, h, C_HUD_DARK);
    
    int padding = 2;
    int segW = 6;
    int gap = 2;
    
    int totalSegments = (w - (2*padding)) / (segW + gap);
    if (totalSegments <= 0) return;

    long activeSegments = ((long)val * totalSegments) / maxVal;
    if (activeSegments > totalSegments) activeSegments = totalSegments;
    
    uint16_t finalColor = activeColor;
    if (val >= (maxVal * 0.9)) finalColor = C_HUD_RED;

    for(int i=0; i<activeSegments; i++) {
        int segX = x + padding + (i * (segW + gap));
        int segY = y + padding;
        int segH = h - (2*padding);
        gfx->fillRect(segX, y + padding, segW, h - (2*padding), finalColor);
    }
}

void drawStaticInterfacePC(Arduino_RGB_Display *gfx) {
    gfx->fillRect(0, 50, 1024, 550, C_BG);

    // Grid
    for (int i = 0; i < 1024; i += 128) gfx->drawFastVLine(i, 50, 550, C_HUD_GRID);
    gfx->drawFastHLine(0, 320, 1024, C_HUD_GRID);

    // 1. CPU
    drawSciFiBracket(gfx, 20, 80, 480, 220, C_HUD_CYAN, "CPU SYSTEM");
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setTextColor(C_HUD_CYAN);
    gfx->setCursor(40, 130); gfx->print("LOAD:");
    gfx->setCursor(40, 210); gfx->print("TEMP:");

    // 2. GPU 
    drawSciFiBracket(gfx, 524, 80, 480, 220, C_HUD_MAGENTA, "GPU CORE");
    gfx->setTextColor(C_HUD_MAGENTA);
    gfx->setCursor(544, 130); gfx->print("LOAD:");
    gfx->setCursor(544, 210); gfx->print("TEMP:");

    // 3. RAM 
    drawSciFiBracket(gfx, 20, 340, 480, 230, C_HUD_YELLOW, "MEMORY USAGE");

    // 4. NET
    drawSciFiBracket(gfx, 524, 340, 480, 230, C_HUD_CYAN, "NETWORK LINK");
}

void refreshDynamicDataPC(Arduino_RGB_Display *gfx) {
    
    // CPU
    drawTechBar(gfx, 40, 140, 320, 20, pcData.cpuLoad, 100, C_HUD_CYAN);
    gfx->fillRect(370, 110, 120, 60, C_BG); 
    gfx->setFont(u8g2_font_logisoso46_tn); gfx->setTextColor(C_HUD_CYAN);
    gfx->setCursor(370, 165); gfx->print(pcData.cpuLoad);
    gfx->setFont(u8g2_font_helvB14_tr); gfx->print("%");

    // Temp
    drawTechBar(gfx, 40, 220, 320, 20, pcData.cpuTemp, 100, C_HUD_MAGENTA);
    gfx->fillRect(370, 190, 120, 60, C_BG); 
    gfx->setFont(u8g2_font_logisoso46_tn); gfx->setTextColor(pcData.cpuTemp > 70 ? C_HUD_RED : C_HUD_MAGENTA);
    gfx->setCursor(370, 245); gfx->print(pcData.cpuTemp);
    gfx->setFont(u8g2_font_helvB14_tr); gfx->print("C");


    // GPU
    drawTechBar(gfx, 544, 140, 320, 20, pcData.gpuLoad, 100, C_HUD_MAGENTA);
    gfx->fillRect(874, 110, 120, 60, C_BG);
    gfx->setFont(u8g2_font_logisoso46_tn); gfx->setTextColor(C_HUD_MAGENTA);
    gfx->setCursor(874, 165); gfx->print(pcData.gpuLoad);
    gfx->setFont(u8g2_font_helvB14_tr); gfx->print("%");

    // Temp
    drawTechBar(gfx, 544, 220, 320, 20, pcData.gpuTemp, 100, C_HUD_MAGENTA);
    gfx->fillRect(874, 190, 120, 60, C_BG); 
    gfx->setFont(u8g2_font_logisoso46_tn); gfx->setTextColor(pcData.gpuTemp > 70 ? C_HUD_RED : C_HUD_MAGENTA);
    gfx->setCursor(874, 245); gfx->print(pcData.gpuTemp);
    gfx->setFont(u8g2_font_helvB14_tr); gfx->print("C");


    // RAM 
    drawTechBar(gfx, 40, 400, 440, 40, pcData.ramUsed, pcData.ramTotal, C_HUD_YELLOW);
    gfx->fillRect(40, 445, 440, 60, C_BG);
    gfx->setFont(u8g2_font_logisoso46_tn); gfx->setTextColor(C_HUD_YELLOW);
    gfx->setCursor(40, 500); gfx->print(pcData.ramUsed); 
    gfx->setFont(u8g2_font_helvB18_tr); gfx->print(" / "); gfx->print(pcData.ramTotal); gfx->print(" GB");


    // NET
    
    // Download
    gfx->fillRect(544, 380, 220, 80, C_BG); 
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setTextColor(C_HUD_CYAN);
    gfx->setCursor(544, 395); gfx->print("DOWNLOAD"); 
    gfx->setFont(u8g2_font_logisoso46_tn);
    gfx->setCursor(544, 455); gfx->print(pcData.netDown); 
    
    // Upload
    gfx->fillRect(780, 380, 220, 80, C_BG); 
    gfx->setFont(u8g2_font_helvB14_tr); gfx->setTextColor(C_HUD_MAGENTA);
    gfx->setCursor(780, 395); gfx->print("UPLOAD"); 
    gfx->setFont(u8g2_font_logisoso46_tn);
    gfx->setCursor(780, 455); gfx->print(pcData.netUp); 

    // Ping
    gfx->fillRect(544, 480, 300, 40, C_BG);
    gfx->setFont(u8g2_font_helvB18_tr); 
    gfx->setTextColor(C_HUD_CYAN); 
    gfx->setCursor(544, 510); 
    gfx->print("PING: "); 
    
    gfx->setTextColor(C_HUD_CYAN);
    gfx->print(pcData.ping); gfx->print(" ms");
    
    gfx->setFont((const GFXfont*)nullptr);
}

void handlePCJson(String json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (!error) {
        pcData.cpuLoad = doc["cpu_load"] | 0;
        pcData.cpuTemp = doc["cpu_temp"] | 0;
        pcData.gpuLoad = doc["gpu_load"] | 0;
        pcData.gpuTemp = doc["gpu_temp"] | 0;
        pcData.ramUsed = doc["ram_used"] | 0;
        pcData.ramTotal = doc["ram_total"] | 32;
        if(pcData.ramTotal == 0) pcData.ramTotal = 32; 
        
        float down = doc["net_down"];
        float up = doc["net_up"];
        pcData.netDown = String(down, 1);
        pcData.netUp = String(up, 1);
        pcData.ping = doc["ping"] | 0;
    }
}
#endif