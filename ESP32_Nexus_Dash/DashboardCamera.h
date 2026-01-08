#ifndef DASHBOARDCAMERA_H
#define DASHBOARDCAMERA_H

#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>

extern Arduino_RGB_Display *gfx;

const char* cameraURL = "http://192.168.1.13:5000/snap.jpg"; 

#define JPG_BUFFER_SIZE 150000

uint8_t* jpgBuffer = NULL;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
   if (y >= 600) return 0;
   
   gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
   return 1;
}

void setupCameraDashboard() {
    TJpgDec.setJpgScale(1);      
    TJpgDec.setSwapBytes(false); 
    TJpgDec.setCallback(tft_output);

    if (jpgBuffer == NULL) {
        jpgBuffer = (uint8_t*)malloc(JPG_BUFFER_SIZE);
    }
}

void drawStaticInterfaceCamera() {
    gfx->fillScreen(0x0000); 
    gfx->setFont(u8g2_font_helvB18_tr);
    gfx->setTextColor(0xFFFF); 
    gfx->setCursor(350, 300);
    gfx->print("PELNY EKRAN...");
    gfx->setFont((const GFXfont*)nullptr);
}

void refreshCameraFrame() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        http.setReuse(true);
        http.begin(cameraURL);
        http.setTimeout(800); 
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            int len = http.getSize();
            
            if (len > 0 && len < JPG_BUFFER_SIZE && jpgBuffer != NULL) {
                WiFiClient *stream = http.getStreamPtr();
                
                int bytesRead = stream->readBytes(jpgBuffer, len);
                if (bytesRead == len) {
                     TJpgDec.drawJpg(0, 0, jpgBuffer, len);
                }
            }
        }
        http.end();
    }
}

#endif