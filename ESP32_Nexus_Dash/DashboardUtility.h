#ifndef DASHBOARDUTILITY_H
#define DASHBOARDUTILITY_H

#include <Arduino_GFX_Library.h>

void drawUtilityInterface(Arduino_RGB_Display *gfx, IPAddress ip) {
    gfx->setFont(u8g2_font_helvB18_tr); 
    gfx->setTextColor(0xFFFF); 
    gfx->setCursor(250, 250);
    gfx->print("UTILITY DASHBOARD");

    gfx->setFont(u8g2_font_helvB14_tr); 
    gfx->setCursor(250, 300);
    gfx->print("Status: Construction in progress...");
    
    gfx->setCursor(250, 340);
    gfx->print("IP: "); 
    gfx->print(ip);
    
    gfx->setFont((const GFXfont*)nullptr);
}

#endif