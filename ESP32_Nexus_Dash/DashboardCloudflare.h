#ifndef DASHBOARDCLOUDFLARE_H
#define DASHBOARDCLOUDFLARE_H

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>

extern Arduino_RGB_Display *gfx;

#define CF_BG           0x0000  // Pure black
#define CF_PANEL        0x0841  // Very dark gray
#define CF_PANEL_LIGHT  0x1082  // Slightly lighter panel
#define CF_BORDER       0x2104  // Subtle border gray

// Neon accents
#define CF_PURPLE       0xA81F  // Neon purple (#A855F7)
#define CF_CYAN         0x07FF  // Neon cyan (#00FFFF)
#define CF_MAGENTA      0xF81F  // Neon magenta (#FF00FF)
#define CF_PINK         0xFC18  // Neon pink (#FF80C0)
#define CF_ORANGE       0xFD20  // Neon orange (#FF9900)

// Status colors
#define CF_GREEN        0x07E0  
#define CF_RED          0xF800  
#define CF_YELLOW       0xFFE0 

// Text
#define CF_WHITE        0xFFFF
#define CF_GRAY         0x8410  
#define CF_DARK_GRAY    0x4208  

volatile unsigned long cfTotalRequests = 0;
volatile unsigned long cfBlockedRequests = 0;
volatile unsigned long cfChallenged = 0;
volatile unsigned long cfThreats = 0;
volatile int cfCacheHitPercent = 0;

volatile int cfWAF = 0;
volatile int cfDDoS = 0;
volatile int cfBot = 0;
volatile int cfRateLimit = 0;
volatile int cfFirewall = 0;

#define CF_MAX_COUNTRIES 5
char cfCountryCodes[CF_MAX_COUNTRIES][4] = {"", "", "", "", ""};
int cfCountryCounts[CF_MAX_COUNTRIES] = {0, 0, 0, 0, 0};

#define CF_MAX_EVENTS 10
char cfEventType[CF_MAX_EVENTS][12] = {"", "", "", "", "", "", "", "", "", ""};
char cfEventCountry[CF_MAX_EVENTS][4] = {"", "", "", "", "", "", "", "", "", ""};
char cfEventAction[CF_MAX_EVENTS][24] = {"", "", "", "", "", "", "", "", "", ""};
char cfEventTime[CF_MAX_EVENTS][10] = {"", "", "", "", "", "", "", "", "", ""};
bool cfEventActive[CF_MAX_EVENTS] = {false, false, false, false, false, false, false, false, false, false};

bool cfDataReady = false;
unsigned long cfLastUpdate = 0;

// HELPERS

const char* getCountryName(const char* code) {
    if (strcmp(code, "US") == 0) return "USA";
    if (strcmp(code, "CN") == 0) return "China";
    if (strcmp(code, "RU") == 0) return "Russia";
    if (strcmp(code, "DE") == 0) return "Germany";
    if (strcmp(code, "FR") == 0) return "France";
    if (strcmp(code, "GB") == 0) return "UK";
    if (strcmp(code, "PL") == 0) return "Poland";
    if (strcmp(code, "BR") == 0) return "Brazil";
    if (strcmp(code, "IN") == 0) return "India";
    if (strcmp(code, "JP") == 0) return "Japan";
    if (strcmp(code, "KR") == 0) return "S.Korea";
    if (strcmp(code, "NL") == 0) return "Netherlands";
    if (strcmp(code, "UA") == 0) return "Ukraine";
    if (strcmp(code, "VN") == 0) return "Vietnam";
    if (strcmp(code, "ID") == 0) return "Indonesia";
    if (strcmp(code, "SG") == 0) return "Singapore";
    if (strcmp(code, "TW") == 0) return "Taiwan";
    if (strcmp(code, "HK") == 0) return "HongKong";
    return code;
}

uint16_t getActionColor(const char* action) {
    if (strcmp(action, "block") == 0) return CF_RED;
    if (strstr(action, "challenge") != NULL) return CF_YELLOW;  
    if (strcmp(action, "managed") == 0) return CF_PURPLE;
    if (strstr(action, "labyrinth") != NULL) return CF_CYAN;
    if (strstr(action, "maze") != NULL) return CF_CYAN;
    if (strstr(action, "served") != NULL) return CF_CYAN;
    return CF_CYAN;
}

// Format action name: capitalize, replace _ with space
const char* getActionName(const char* action) {
    // Use strstr for partial matches
    if (strcmp(action, "block") == 0) return "Block";
    if (strcmp(action, "challenge") == 0) return "Challenge";
    if (strcmp(action, "managed") == 0) return "Managed";
    if (strcmp(action, "skip") == 0) return "Skip";
    if (strcmp(action, "log") == 0) return "Log";
    if (strcmp(action, "allow") == 0) return "Allow";
    if (strcmp(action, "bypass") == 0) return "Bypass";
    if (strcmp(action, "served") == 0) return "Served";
    // Partial matches for longer/truncated names
    if (strstr(action, "managed_c") != NULL) return "Managed";
    if (strstr(action, "js_chall") != NULL) return "JS Challenge";
    if (strstr(action, "jschall") != NULL) return "JS Challenge";
    if (strstr(action, "labyrinth") != NULL) return "AI Labyrinth";
    if (strstr(action, "maze") != NULL) return "AI Labyrinth";
    if (strstr(action, "connection") != NULL) return "Conn Close";
    // Return original if not matched
    return action;
}

uint16_t getTypeColor(const char* type) {
    if (strcmp(type, "WAF") == 0) return CF_PURPLE;
    if (strcmp(type, "DDoS") == 0) return CF_ORANGE;
    if (strcmp(type, "Bot") == 0) return CF_RED;
    if (strcmp(type, "RateLimit") == 0) return CF_CYAN;
    if (strcmp(type, "Firewall") == 0) return CF_GREEN;
    return CF_WHITE;
}

void drawNeonPanel(int x, int y, int w, int h, uint16_t accentColor) {
    gfx->fillRect(x, y, w, h, CF_PANEL);
    gfx->fillRect(x, y, w, 2, accentColor);
    gfx->drawFastVLine(x, y, h, CF_BORDER);
    gfx->drawFastVLine(x + w - 1, y, h, CF_BORDER);
    gfx->drawFastHLine(x, y + h - 1, w, CF_BORDER);
}

void drawNeonBar(int x, int y, int w, int h, int val, int maxVal, uint16_t color) {
    if (maxVal <= 0) maxVal = 1;
    
    gfx->fillRect(x, y, w, h, CF_PANEL_LIGHT);
    
    int fillW = ((long)val * w) / maxVal;
    if (fillW > w) fillW = w;
    if (fillW < 0) fillW = 0;
    
    if (fillW > 0) {
        gfx->fillRect(x, y, fillW, h, color);
    }
}

// Mini stat card
void drawStatCard(int x, int y, int w, const char* label, unsigned long value, uint16_t color) {
    gfx->fillRect(x, y, w, 80, CF_PANEL);
    gfx->fillRect(x, y, w, 3, color);
    
    gfx->setFont(u8g2_font_helvB10_tr);
    gfx->setTextColor(CF_GRAY);
    gfx->setCursor(x + 10, y + 25);
    gfx->print(label);
    
    gfx->fillRect(x + 10, y + 35, w - 20, 35, CF_PANEL);
    gfx->setFont(u8g2_font_logisoso24_tn);
    gfx->setTextColor(color);
    gfx->setCursor(x + 10, y + 68);
    
    if (value > 999999) {
        gfx->print(value / 1000000.0, 1);
        gfx->setFont(u8g2_font_helvB12_tr);
        gfx->print("M");
    } else if (value > 9999) {
        gfx->print(value / 1000.0, 1);
        gfx->setFont(u8g2_font_helvB12_tr);
        gfx->print("K");
    } else {
        gfx->print(value);
    }
    gfx->setFont((const GFXfont*)nullptr);
}

// Event row
void drawEventRow(int x, int y, int index) {
    if (index < 0 || index >= CF_MAX_EVENTS || !cfEventActive[index]) return;
    
    int rowH = 48;
    
    gfx->fillRect(x, y, 590, rowH, CF_PANEL);
    
    uint16_t typeColor = getTypeColor(cfEventType[index]);
    gfx->fillRect(x, y, 4, rowH, typeColor);
    
    // Time
    gfx->setFont(u8g2_font_helvB10_tr);
    gfx->setTextColor(CF_DARK_GRAY);
    gfx->setCursor(x + 15, y + 20);
    gfx->print(cfEventTime[index]);
    
    // Type
    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(typeColor);
    gfx->setCursor(x + 100, y + 20);
    gfx->print(cfEventType[index]);
    
    // Country
    gfx->setTextColor(CF_WHITE);
    gfx->setCursor(x + 200, y + 20);
    gfx->print(getCountryName(cfEventCountry[index]));
    
    // Action
    uint16_t actionColor = getActionColor(cfEventAction[index]);
    gfx->setFont(u8g2_font_helvB10_tr);
    gfx->setTextColor(actionColor);
    gfx->setCursor(x + 345, y + 20);
    gfx->print(getActionName(cfEventAction[index]));
    
    gfx->drawFastHLine(x + 10, y + rowH - 1, 570, CF_BORDER);
    gfx->setFont((const GFXfont*)nullptr);
}

// Main panels

void drawTopBar() {
    gfx->fillRect(0, 0, 1024, 45, CF_PANEL);
    gfx->fillRect(0, 45, 1024, 2, CF_PURPLE);
    
    gfx->setFont(u8g2_font_helvB12_tr);
    
    gfx->setTextColor(CF_WHITE);
    gfx->setCursor(20, 30);
    gfx->print("SECURITY MONITOR");
    
    gfx->setTextColor(CF_WHITE);
    gfx->setCursor(447, 30);
    gfx->print("KrzysztofWasik.com");
    
    gfx->setTextColor(CF_GREEN);
    gfx->setCursor(945, 30);
    gfx->print("LIVE");
    gfx->fillCircle(998, 24, 5, CF_GREEN);
    
    gfx->setFont((const GFXfont*)nullptr);
}

void drawEventsPanel() {
    int x = 10;
    int y = 55;
    
    gfx->fillRect(x, y, 600, 535, CF_PANEL);
    gfx->drawRect(x, y, 600, 535, CF_BORDER);
    
    gfx->setFont(u8g2_font_helvB10_tr);
    gfx->setTextColor(CF_DARK_GRAY);
    gfx->setCursor(x + 15, y + 18);
    gfx->print("TIME");
    gfx->setCursor(x + 100, y + 18);
    gfx->print("TYPE");
    gfx->setCursor(x + 200, y + 18);
    gfx->print("SOURCE");
    gfx->setCursor(x + 350, y + 18);
    gfx->print("ACTION");
    
    gfx->drawFastHLine(x + 10, y + 25, 580, CF_BORDER);
    gfx->setFont((const GFXfont*)nullptr);
}

void drawStatsPanel() {
    int x = 620;
    int y = 55;
    int w = 395;
    
    gfx->fillRect(x, y, w, 100, CF_PANEL);
    gfx->drawRect(x, y, w, 100, CF_BORDER);
    
    drawStatCard(x + 10, y + 10, 185, "REQUESTS", cfTotalRequests, CF_CYAN);
    drawStatCard(x + 200, y + 10, 185, "BLOCKED", cfBlockedRequests, CF_RED);
}

void drawThreatTypesPanel() {
    int x = 620;
    int y = 165;
    int w = 395;
    
    drawNeonPanel(x, y, w, 150, CF_PURPLE);
    
    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(CF_PURPLE);
    gfx->setCursor(x + 15, y + 25);
    gfx->print("THREAT BREAKDOWN");
    gfx->setFont((const GFXfont*)nullptr);
    
    int maxCount = cfWAF;
    if (cfDDoS > maxCount) maxCount = cfDDoS;
    if (cfBot > maxCount) maxCount = cfBot;
    if (cfRateLimit > maxCount) maxCount = cfRateLimit;
    if (maxCount < 1) maxCount = 1;
    
    int barY = y + 40;
    int barH = 14;
    int labelW = 70;
    int barW = 230;
    int countX = x + labelW + barW + 15;
    
    gfx->setFont(u8g2_font_helvB10_tr);
    
    // WAF
    gfx->setTextColor(CF_PURPLE);
    gfx->setCursor(x + 15, barY + 11);
    gfx->print("WAF");
    drawNeonBar(x + labelW, barY, barW, barH, cfWAF, maxCount, CF_PURPLE);
    gfx->setCursor(countX, barY + 11);
    gfx->print(cfWAF);
    barY += 22;
    
    // DDoS
    gfx->setTextColor(CF_ORANGE);
    gfx->setCursor(x + 15, barY + 11);
    gfx->print("DDoS");
    drawNeonBar(x + labelW, barY, barW, barH, cfDDoS, maxCount, CF_RED);
    gfx->setCursor(countX, barY + 11);
    gfx->print(cfDDoS);
    barY += 22;
    
    // Bot
    gfx->setTextColor(CF_RED);
    gfx->setCursor(x + 15, barY + 11);
    gfx->print("Bot");
    drawNeonBar(x + labelW, barY, barW, barH, cfBot, maxCount, CF_ORANGE);
    gfx->setCursor(countX, barY + 11);
    gfx->print(cfBot);
    barY += 22;
    
    // Rate Limit
    gfx->setTextColor(CF_CYAN);
    gfx->setCursor(x + 15, barY + 11);
    gfx->print("Rate");
    drawNeonBar(x + labelW, barY, barW, barH, cfRateLimit, maxCount, CF_CYAN);
    gfx->setCursor(countX, barY + 11);
    gfx->print(cfRateLimit);
    
    gfx->setFont((const GFXfont*)nullptr);
}

void drawTopCountriesPanel() {
    int x = 620;
    int y = 325;
    int w = 395;
    
    drawNeonPanel(x, y, w, 265, CF_CYAN);
    
    gfx->setFont(u8g2_font_helvB12_tr);
    
    // Headers
    gfx->setTextColor(CF_CYAN);
    gfx->setCursor(x + 15, y + 25);
    gfx->print("TOP SOURCES");
    
    int rightX = x + 185;
    gfx->drawFastVLine(rightX - 5, y + 10, 245, CF_BORDER);
    
    gfx->setTextColor(CF_PURPLE);
    gfx->setCursor(rightX + 10, y + 25);
    gfx->print("TOTAL STATS");
    
    // Rows
    int rowY = y + 50;
    int rowGap = 40;
    
    // Row 1
    int cy = rowY;
    if (cfCountryCodes[0][0] != '\0') {
        gfx->fillCircle(x + 25, cy + 5, 11, CF_PANEL_LIGHT);
        gfx->setTextColor(CF_WHITE);
        gfx->setCursor(x + 21, cy + 11);
        gfx->print("1");
        gfx->setTextColor(CF_GRAY);
        gfx->setCursor(x + 45, cy + 11);
        gfx->print(getCountryName(cfCountryCodes[0]));
        gfx->setTextColor(CF_CYAN);
        gfx->setCursor(x + 135, cy + 11);
        gfx->print(cfCountryCounts[0]);
    }
    gfx->setTextColor(CF_GRAY);
    gfx->setCursor(rightX + 10, cy + 11);
    gfx->print("Requests");
    gfx->setTextColor(CF_CYAN);
    gfx->setCursor(rightX + 105, cy + 11);
    if (cfTotalRequests > 999999) {
        gfx->print(cfTotalRequests / 1000000.0, 1);
        gfx->print("M");
    } else if (cfTotalRequests > 9999) {
        gfx->print(cfTotalRequests / 1000.0, 1);
        gfx->print("K");
    } else {
        gfx->print(cfTotalRequests);
    }
    
    // Row 2
    cy = rowY + rowGap;
    if (cfCountryCodes[1][0] != '\0') {
        gfx->fillCircle(x + 25, cy + 5, 11, CF_PANEL_LIGHT);
        gfx->setTextColor(CF_WHITE);
        gfx->setCursor(x + 21, cy + 11);
        gfx->print("2");
        gfx->setTextColor(CF_GRAY);
        gfx->setCursor(x + 45, cy + 11);
        gfx->print(getCountryName(cfCountryCodes[1]));
        gfx->setTextColor(CF_CYAN);
        gfx->setCursor(x + 135, cy + 11);
        gfx->print(cfCountryCounts[1]);
    }
    gfx->setTextColor(CF_GRAY);
    gfx->setCursor(rightX + 10, cy + 11);
    gfx->print("Blocked");
    gfx->setTextColor(CF_RED);
    gfx->setCursor(rightX + 105, cy + 11);
    gfx->print(cfBlockedRequests);
    
    // Row 3
    cy = rowY + rowGap * 2;
    if (cfCountryCodes[2][0] != '\0') {
        gfx->fillCircle(x + 25, cy + 5, 11, CF_PANEL_LIGHT);
        gfx->setTextColor(CF_WHITE);
        gfx->setCursor(x + 21, cy + 11);
        gfx->print("3");
        gfx->setTextColor(CF_GRAY);
        gfx->setCursor(x + 45, cy + 11);
        gfx->print(getCountryName(cfCountryCodes[2]));
        gfx->setTextColor(CF_CYAN);
        gfx->setCursor(x + 135, cy + 11);
        gfx->print(cfCountryCounts[2]);
    }
    gfx->setTextColor(CF_GRAY);
    gfx->setCursor(rightX + 10, cy + 11);
    gfx->print("Threats");
    gfx->setTextColor(CF_MAGENTA);
    gfx->setCursor(rightX + 105, cy + 11);
    gfx->print(cfThreats);
    
    // Row 4
    cy = rowY + rowGap * 3;
    if (cfCountryCodes[3][0] != '\0') {
        gfx->fillCircle(x + 25, cy + 5, 11, CF_PANEL_LIGHT);
        gfx->setTextColor(CF_WHITE);
        gfx->setCursor(x + 21, cy + 11);
        gfx->print("4");
        gfx->setTextColor(CF_GRAY);
        gfx->setCursor(x + 45, cy + 11);
        gfx->print(getCountryName(cfCountryCodes[3]));
        gfx->setTextColor(CF_CYAN);
        gfx->setCursor(x + 135, cy + 11);
        gfx->print(cfCountryCounts[3]);
    }
    gfx->setTextColor(CF_GRAY);
    gfx->setCursor(rightX + 10, cy + 11);
    gfx->print("Challenged");
    gfx->setTextColor(CF_YELLOW);
    gfx->setCursor(rightX + 105, cy + 11);
    gfx->print(cfChallenged);
    
    // Row 5
    cy = rowY + rowGap * 4;
    if (cfCountryCodes[4][0] != '\0') {
        gfx->fillCircle(x + 25, cy + 5, 11, CF_PANEL_LIGHT);
        gfx->setTextColor(CF_WHITE);
        gfx->setCursor(x + 21, cy + 11);
        gfx->print("5");
        gfx->setTextColor(CF_GRAY);
        gfx->setCursor(x + 45, cy + 11);
        gfx->print(getCountryName(cfCountryCodes[4]));
        gfx->setTextColor(CF_CYAN);
        gfx->setCursor(x + 135, cy + 11);
        gfx->print(cfCountryCounts[4]);
    }
    gfx->setTextColor(CF_GRAY);
    gfx->setCursor(rightX + 10, cy + 11);
    gfx->print("Cache Hit");
    gfx->setTextColor(CF_GREEN);
    gfx->setCursor(rightX + 105, cy + 11);
    gfx->print(cfCacheHitPercent);
    gfx->print("%");
    
    gfx->setFont((const GFXfont*)nullptr);
}

void drawStaticInterfaceCloudflare() {
    gfx->fillScreen(CF_BG);
    yield();
    
    drawTopBar();
    yield();
    
    drawEventsPanel();
    yield();
    
    drawStatsPanel();
    yield();
    
    drawThreatTypesPanel();
    yield();
    
    drawTopCountriesPanel();
    yield();
    
    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(CF_DARK_GRAY);
    gfx->setCursor(220, 300);
    gfx->print("Waiting for data...");
    gfx->setFont((const GFXfont*)nullptr);
}

void refreshDynamicDataCloudflare() {
    static unsigned long lastPulse = 0;
    static bool pulse = false;
    
    if (millis() - lastPulse > 500) {
        pulse = !pulse;
        gfx->fillCircle(998, 24, 5, pulse ? CF_GREEN : CF_PANEL);
        lastPulse = millis();
    }
    
    if (!cfDataReady) {
        delay(50);
        return;
    }
    
    gfx->fillRect(15, 85, 590, 500, CF_PANEL);
    
    int eventY = 90;
    for (int i = 0; i < CF_MAX_EVENTS; i++) {
        if (cfEventActive[i]) {
            drawEventRow(15, eventY, i);
            eventY += 49;
        }
    }
    yield();
    
    // Update stats
    drawStatsPanel();
    yield();
    
    drawThreatTypesPanel();
    yield();
    
    drawTopCountriesPanel();
    
    cfDataReady = false;
    delay(100);
}

void handleCloudflareJson(String json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.println("CF JSON parse error");
        return;
    }
    
    cfTotalRequests = doc["total_requests"] | 0;
    cfBlockedRequests = doc["blocked"] | 0;
    cfChallenged = doc["challenged"] | 0;
    cfThreats = doc["threats"] | 0;
    cfCacheHitPercent = doc["cache_hit"] | 0;
    
    cfWAF = doc["waf"] | 0;
    cfDDoS = doc["ddos"] | 0;
    cfBot = doc["bot"] | 0;
    cfRateLimit = doc["rate_limit"] | 0;
    cfFirewall = doc["firewall"] | 0;
    
    JsonArray countries = doc["top_countries"];
    for (int i = 0; i < CF_MAX_COUNTRIES; i++) {
        cfCountryCodes[i][0] = '\0';
        cfCountryCounts[i] = 0;
    }
    
    int idx = 0;
    for (JsonObject country : countries) {
        if (idx >= CF_MAX_COUNTRIES) break;
        const char* code = country["code"] | "";
        strncpy(cfCountryCodes[idx], code, 3);
        cfCountryCodes[idx][3] = '\0';
        cfCountryCounts[idx] = country["count"] | 0;
        idx++;
    }
    
    JsonArray events = doc["events"];
    for (int i = 0; i < CF_MAX_EVENTS; i++) {
        cfEventActive[i] = false;
        cfEventType[i][0] = '\0';
        cfEventCountry[i][0] = '\0';
        cfEventAction[i][0] = '\0';
        cfEventTime[i][0] = '\0';
    }
    
    idx = 0;
    for (JsonObject event : events) {
        if (idx >= CF_MAX_EVENTS) break;
        
        const char* type = event["type"] | "";
        const char* country = event["country"] | "";
        const char* action = event["action"] | "";
        const char* time = event["time"] | "";
        
        strncpy(cfEventType[idx], type, 11);
        cfEventType[idx][11] = '\0';
        strncpy(cfEventCountry[idx], country, 3);
        cfEventCountry[idx][3] = '\0';
        strncpy(cfEventAction[idx], action, 9);
        cfEventAction[idx][9] = '\0';
        strncpy(cfEventTime[idx], time, 9);
        cfEventTime[idx][9] = '\0';
        cfEventActive[idx] = true;
        
        idx++;
    }
    
    cfLastUpdate = millis();
    cfDataReady = true;
}

#endif
