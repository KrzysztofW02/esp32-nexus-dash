#ifndef DASHBOARDYOUTUBE_H
#define DASHBOARDYOUTUBE_H

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>

extern Arduino_RGB_Display *gfx;

#define YT_BG           0x0000  
#define YT_PANEL        0x0841  
#define YT_BORDER       0x2104 
#define YT_RED          0xF800 
#define YT_WHITE        0xFFFF
#define YT_GRAY         0x8410
#define YT_DARK_GRAY    0x4208

volatile unsigned long ytSubscribers = 0;
volatile unsigned long ytTotalViews = 0;
volatile unsigned long ytVideoCount = 0;

char ytChannelName[26] = "";

#define YT_MAX_COMMENTS 5
char ytCommentAuthor[YT_MAX_COMMENTS][20] = {"","","","",""};
char ytCommentText[YT_MAX_COMMENTS][90] = {"","","","",""};
char ytCommentTime[YT_MAX_COMMENTS][8] = {"","","","",""};
bool ytCommentActive[YT_MAX_COMMENTS] = {false, false, false, false, false};

bool ytDataReady = false;
unsigned long ytLastUpdate = 0;

void ytDrawPlayIcon(int x, int y) {
    gfx->fillRoundRect(x, y, 30, 22, 5, YT_RED);
    gfx->fillTriangle(x + 11, y + 5, x + 11, y + 17, x + 22, y + 11, YT_WHITE);
}

extern String getLocalTimeStr();
void ytUpdateClock() {
    String timeStr = getLocalTimeStr();
    gfx->fillRect(430, 2, 170, 41, YT_PANEL);
    gfx->setFont(u8g2_font_profont29_mn);
    gfx->setTextSize(1);
    gfx->setTextColor(YT_WHITE);
    gfx->setCursor(444, 35);
    gfx->print(timeStr);
    gfx->setFont((const GFXfont*)nullptr);
}

void ytPrintCenteredNum(int x, int y, int w, unsigned long value, int charW) {
    char raw[16];
    snprintf(raw, sizeof(raw), "%lu", value);
    int len = strlen(raw);

    int numSpaces = (len > 1) ? (len - 1) / 3 : 0;
    int spaceW = charW / 3;
    int totalW = len * charW + numSpaces * spaceW;
    int cx = x + (w - totalW) / 2;

    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) cx += spaceW;
        gfx->setCursor(cx, y);
        char c[2] = {raw[i], '\0'};
        gfx->print(c);
        cx += charW;
    }
}

void ytDrawStatPanel(int x, int y, int w, int h, const char* label, unsigned long value) {
    gfx->fillRect(x, y, w, h, YT_PANEL);
    gfx->fillRect(x, y, w, 3, YT_DARK_GRAY);
    gfx->drawFastVLine(x, y, h, YT_BORDER);
    gfx->drawFastVLine(x + w - 1, y, h, YT_BORDER);
    gfx->drawFastHLine(x, y + h - 1, w, YT_BORDER);

    // Label
    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(YT_GRAY);
    int labelWidth = strlen(label) * 9;
    int labelX = x + (w - labelWidth) / 2;
    gfx->setCursor(labelX, y + 30);
    gfx->print(label);

    // Number
    gfx->setFont(u8g2_font_logisoso58_tn);
    gfx->setTextColor(YT_WHITE);
    int valueY = y + 40 + ((h - 40) / 2) + 22;
    ytPrintCenteredNum(x, valueY, w, value, 37);

    gfx->setFont((const GFXfont*)nullptr);
}

void ytDrawCommentRow(int x, int y, int index) {
    if (index < 0 || index >= YT_MAX_COMMENTS || !ytCommentActive[index]) return;

    int rowH = 62;
    int rowW = 1004;

    gfx->fillRect(x, y, rowW, rowH, YT_PANEL);

    // Red accent stripe on left
    gfx->fillRect(x, y, 5, rowH, YT_RED);

    // Author name
    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(YT_RED);
    gfx->setCursor(x + 18, y + 37);
    gfx->print(ytCommentAuthor[index]);

    // Comment text
    gfx->setTextColor(YT_WHITE);
    gfx->setCursor(x + 180, y + 35);
    gfx->print(ytCommentText[index]);

    // Time ago
    gfx->setFont(u8g2_font_helvB10_tr);
    gfx->setTextColor(YT_DARK_GRAY);
    gfx->setCursor(x + 930, y + 35);
    gfx->print(ytCommentTime[index]);

    gfx->drawFastHLine(x + 10, y + rowH - 1, rowW - 20, YT_BORDER);

    gfx->setFont((const GFXfont*)nullptr);
}

// Top Bar
void ytDrawTopBar() {
    gfx->fillRect(0, 0, 1024, 45, YT_PANEL);
    gfx->fillRect(0, 45, 1024, 2, YT_RED);

    ytDrawPlayIcon(15, 12);

    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(YT_WHITE);
    gfx->setCursor(52, 31);
    gfx->print("YOUTUBE DASHBOARD");

    gfx->setTextColor(YT_RED);
    gfx->setCursor(930, 31);
    gfx->print("LIVE");
    gfx->fillCircle(988, 25, 5, YT_RED);

    gfx->setFont((const GFXfont*)nullptr);
}

void ytDrawCommentsPanel() {
    int x = 10;
    int y = 255;
    int w = 1004;
    int h = 335;

    gfx->fillRect(x, y, w, h, YT_PANEL);
    gfx->drawRect(x, y, w, h, YT_BORDER);

    // Column headers
    gfx->setFont(u8g2_font_helvB10_tr);
    gfx->setTextColor(YT_DARK_GRAY);
    gfx->setCursor(x + 23, y + 18);
    gfx->print("AUTHOR");
    gfx->setCursor(x + 180, y + 18);
    gfx->print("COMMENT");
    gfx->setCursor(x + 935, y + 18);
    gfx->print("WHEN");

    gfx->drawFastHLine(x + 10, y + 25, w - 20, YT_BORDER);

    gfx->setFont((const GFXfont*)nullptr);
}

void drawStaticInterfaceYouTube() {
    gfx->fillScreen(YT_BG);
    yield();

    ytDrawTopBar();
    yield();

    ytDrawStatPanel(10, 55, 497, 190, "SUBSCRIBERS", ytSubscribers);
    yield();

    ytDrawStatPanel(517, 55, 497, 190, "TOTAL VIEWS", ytTotalViews);
    yield();

    ytDrawCommentsPanel();
    yield();

    // Placeholder
    gfx->setFont(u8g2_font_helvB12_tr);
    gfx->setTextColor(YT_DARK_GRAY);
    gfx->setCursor(410, 440);
    gfx->print("Waiting for data...");
    gfx->setFont((const GFXfont*)nullptr);
}

// Dynamic Refresh
void refreshDynamicDataYouTube() {
    static unsigned long lastPulse = 0;
    static unsigned long lastClock = 0;
    static bool pulse = false;

    unsigned long now = millis();

    if (now - lastClock > 1000) {
        lastClock = now;
        ytUpdateClock();
    }

    if (now - lastPulse > 500) {
        pulse = !pulse;
        gfx->fillCircle(988, 25, 5, pulse ? YT_RED : YT_PANEL);
        lastPulse = now;
    }

    if (!ytDataReady) {
        delay(50);
        return;
    }

    ytDrawStatPanel(10, 55, 497, 190, "SUBSCRIBERS", ytSubscribers);
    yield();

    ytDrawStatPanel(517, 55, 497, 190, "TOTAL VIEWS", ytTotalViews);
    yield();

    gfx->fillRect(15, 285, 994, 300, YT_PANEL);

    int commentY = 290;
    for (int i = 0; i < YT_MAX_COMMENTS; i++) {
        if (ytCommentActive[i]) {
            ytDrawCommentRow(15, commentY, i);
            commentY += 63;
        }
    }

    ytDataReady = false;
    delay(100);
}

void handleYouTubeJson(String json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println("YT JSON parse error");
        return;
    }

    ytSubscribers = doc["subscribers"] | 0;
    ytTotalViews = doc["total_views"] | 0;
    ytVideoCount = doc["video_count"] | 0;

    const char* name = doc["channel_name"] | "";
    strncpy(ytChannelName, name, 25);
    ytChannelName[25] = '\0';

    for (int i = 0; i < YT_MAX_COMMENTS; i++) {
        ytCommentActive[i] = false;
        ytCommentAuthor[i][0] = '\0';
        ytCommentText[i][0] = '\0';
        ytCommentTime[i][0] = '\0';
    }

    JsonArray comments = doc["comments"];
    int idx = 0;
    for (JsonObject comment : comments) {
        if (idx >= YT_MAX_COMMENTS) break;

        const char* author = comment["author"] | "";
        const char* text = comment["text"] | "";
        const char* ctime = comment["time"] | "";

        strncpy(ytCommentAuthor[idx], author, 19);
        ytCommentAuthor[idx][19] = '\0';
        strncpy(ytCommentText[idx], text, 89);
        ytCommentText[idx][89] = '\0';
        strncpy(ytCommentTime[idx], ctime, 7);
        ytCommentTime[idx][7] = '\0';
        ytCommentActive[idx] = true;

        idx++;
    }

    ytLastUpdate = millis();
    ytDataReady = true;
}

#endif
