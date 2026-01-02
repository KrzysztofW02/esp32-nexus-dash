#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include <WebServer.h>

extern WebServer server;
extern int currentDashboard;
extern bool needRedrawStatic;

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Dashboard Controller</title>";
  html += "<style>";
  // Dark theme 
  html += "body { font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 0; display: flex; flex-direction: column; align-items: center; min-height: 100vh; }";
  html += "h1 { margin-top: 40px; font-weight: 300; letter-spacing: 2px; text-transform: uppercase; color: #00e5ff; }";
  html += ".container { width: 90%; max-width: 600px; margin-top: 20px; display: grid; gap: 20px; }";
  // Card
  html += ".card { background: #1e1e1e; border: 1px solid #333; border-radius: 12px; padding: 25px; text-align: center; transition: transform 0.2s, box-shadow 0.2s, border-color 0.2s; cursor: pointer; text-decoration: none; color: inherit; display: block; }";
  html += ".card:hover { transform: translateY(-5px); box-shadow: 0 10px 20px rgba(0,0,0,0.5); border-color: #00e5ff; }";
  // Active state
  html += ".active { border-color: #00e5ff; background: #252525; box-shadow: 0 0 15px rgba(0, 229, 255, 0.1); }";
  // Content inside card
  html += ".icon { font-size: 40px; margin-bottom: 15px; display: block; }";
  html += ".btn-title { font-size: 1.2rem; font-weight: bold; display: block; margin-bottom: 5px; }";
  html += ".btn-desc { font-size: 0.9rem; color: #aaa; display: block; }";
  html += "</style></head><body>";
  
  html += "<h1>System Control</h1>";
  html += "<div class='container'>";
  
  // Card 1
  html += "<a href='/set?mode=0' class='card " + String(currentDashboard == 0 ? "active" : "") + "'>";
  html += "<span class='icon'>💰</span>";
  html += "<span class='btn-title'>Finance Monitor</span>"; 
  html += "<span class='btn-desc'>Live BTC, ETH, USD, EUR rates with history charts.</span>";
  html += "</a>";
  
  // Card 2
  html += "<a href='/set?mode=1' class='card " + String(currentDashboard == 1 ? "active" : "") + "'>";
  html += "<span class='icon'>🛠️</span>";
  html += "<span class='btn-title'>Utility Dashboard</span>";
  html += "<span class='btn-desc'>(In Construction).</span>";
  html += "</a>";
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("mode")) {
    int newMode = server.arg("mode").toInt();
    if (newMode != currentDashboard) {
       currentDashboard = newMode;
       needRedrawStatic = true; 
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

#endif