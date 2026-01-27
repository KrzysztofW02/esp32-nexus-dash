#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include <WebServer.h>
#include "DashboardPC.h" 
#include "DashboardCloudflare.h"

extern WebServer server;
extern int currentDashboard;
extern bool needRedrawStatic;
extern bool dataReadyToDraw;

void handleUpdatePC() {
    if (currentDashboard != 1) {
        server.send(200, "text/plain", "Ignored - Dashboard inactive");
        return; 
    }

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        handlePCJson(body);
        dataReadyToDraw = true; 
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "No Data");
    }
}

void handleUpdateCloudflare() {
    if (currentDashboard != 3) {
        server.send(200, "text/plain", "Ignored - Dashboard inactive");
        return; 
    }

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        handleCloudflareJson(body);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "No Data");
    }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Nexus Dashboard</title>";
  html += "<link rel='preconnect' href='https://fonts.googleapis.com'>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap' rel='stylesheet'>";
  html += "<style>";
  
  // CSS Variables & Reset
  html += ":root{--bg-primary:#0a0a0f;--bg-secondary:#12121a;--bg-card:#16161f;--bg-card-hover:#1a1a25;--border:#252530;--border-hover:#3a3a4a;--accent-cyan:#00d4ff;--accent-purple:#a855f7;--accent-pink:#ec4899;--accent-orange:#f97316;--accent-green:#22c55e;--text-primary:#f1f1f3;--text-secondary:#9ca3af;--text-muted:#6b7280;--glow-cyan:0 0 20px rgba(0,212,255,0.3);--glow-purple:0 0 20px rgba(168,85,247,0.3);--glow-pink:0 0 20px rgba(236,72,153,0.3);--glow-orange:0 0 20px rgba(249,115,22,0.3);}";
  html += "*{margin:0;padding:0;box-sizing:border-box;}";
  
  // Body with animated gradient
  html += "body{font-family:'Inter',system-ui,-apple-system,sans-serif;background:var(--bg-primary);color:var(--text-primary);min-height:100vh;overflow-x:hidden;position:relative;}";
  html += "body::before{content:'';position:fixed;top:0;left:0;right:0;bottom:0;background:radial-gradient(ellipse at 50% 0%,rgba(0,212,255,0.08) 0%,transparent 50%),radial-gradient(ellipse at 80% 50%,rgba(168,85,247,0.06) 0%,transparent 40%),radial-gradient(ellipse at 20% 80%,rgba(236,72,153,0.05) 0%,transparent 40%);pointer-events:none;z-index:0;}";
  
  // Main container
  html += ".main{position:relative;z-index:1;max-width:900px;margin:0 auto;padding:40px 20px 60px;}";
  
  // Header
  html += ".header{text-align:center;margin-bottom:50px;}";
  html += ".logo{display:inline-flex;align-items:center;gap:12px;margin-bottom:16px;}";
  html += ".logo-icon{width:48px;height:48px;background:linear-gradient(135deg,var(--accent-cyan),var(--accent-purple));border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:24px;box-shadow:var(--glow-cyan);}";
  html += ".logo-text{font-size:28px;font-weight:700;background:linear-gradient(135deg,var(--accent-cyan),var(--accent-purple));-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;letter-spacing:-0.5px;}";
  html += ".subtitle{color:var(--text-muted);font-size:14px;font-weight:400;letter-spacing:0.5px;}";
  
  // Grid
  html += ".grid{display:grid;grid-template-columns:repeat(2,1fr);gap:20px;}";
  html += "@media(max-width:640px){.grid{grid-template-columns:1fr;}}";
  
  // Cards
  html += ".card{position:relative;background:var(--bg-card);border:1px solid var(--border);border-radius:20px;padding:28px;text-decoration:none;color:inherit;display:flex;flex-direction:column;transition:all 0.3s cubic-bezier(0.4,0,0.2,1);overflow:hidden;}";
  html += ".card::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;background:linear-gradient(90deg,var(--card-color),transparent);opacity:0;transition:opacity 0.3s;}";
  html += ".card::after{content:'';position:absolute;top:0;left:0;right:0;bottom:0;background:radial-gradient(circle at 50% 0%,var(--card-glow),transparent 70%);opacity:0;transition:opacity 0.3s;pointer-events:none;}";
  html += ".card:hover{background:var(--bg-card-hover);border-color:var(--border-hover);transform:translateY(-4px);}";
  html += ".card:hover::before,.card:hover::after{opacity:1;}";
  
  // Active card
  html += ".card.active{border-color:var(--card-color);background:var(--bg-card-hover);}";
  html += ".card.active::before{opacity:1;}";
  html += ".card.active .badge{opacity:1;}";
  
  // Card colors
  html += ".card.finance{--card-color:var(--accent-cyan);--card-glow:rgba(0,212,255,0.1);}";
  html += ".card.pc{--card-color:var(--accent-purple);--card-glow:rgba(168,85,247,0.1);}";
  html += ".card.camera{--card-color:var(--accent-pink);--card-glow:rgba(236,72,153,0.1);}";
  html += ".card.security{--card-color:var(--accent-orange);--card-glow:rgba(249,115,22,0.1);}";
  
  // Card content
  html += ".card-header{display:flex;align-items:flex-start;justify-content:space-between;margin-bottom:16px;}";
  html += ".icon-wrap{width:52px;height:52px;background:linear-gradient(135deg,color-mix(in srgb,var(--card-color) 20%,transparent),color-mix(in srgb,var(--card-color) 5%,transparent));border:1px solid color-mix(in srgb,var(--card-color) 30%,transparent);border-radius:14px;display:flex;align-items:center;justify-content:center;font-size:26px;transition:all 0.3s;}";
  html += ".card:hover .icon-wrap{transform:scale(1.05);box-shadow:0 0 20px color-mix(in srgb,var(--card-color) 30%,transparent);}";
  html += ".badge{font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:1px;padding:4px 10px;border-radius:100px;background:color-mix(in srgb,var(--card-color) 15%,transparent);color:var(--card-color);opacity:0;transition:opacity 0.3s;}";
  html += ".card-title{font-size:18px;font-weight:600;margin-bottom:8px;color:var(--text-primary);}";
  html += ".card-desc{font-size:14px;color:var(--text-secondary);line-height:1.5;flex-grow:1;}";
  html += ".card-footer{display:flex;align-items:center;justify-content:space-between;margin-top:20px;padding-top:16px;border-top:1px solid var(--border);}";
  html += ".card-action{font-size:13px;font-weight:500;color:var(--card-color);display:flex;align-items:center;gap:6px;transition:gap 0.3s;}";
  html += ".card:hover .card-action{gap:10px;}";
  html += ".arrow{transition:transform 0.3s;}";
  html += ".card:hover .arrow{transform:translateX(4px);}";
  
  html += "</style></head><body>";
  
  html += "<div class='main'>";
  
  // Header
  html += "<header class='header'>";
  html += "<div class='logo'><span class='logo-text'>NEXUS DASH</span></div>";
  html += "<p class='subtitle'>ESP32 Smart Display Controller</p>";
  html += "</header>";
  
  // Cards grid
  html += "<div class='grid'>";
  
  // Card 1 - Finance
  html += "<a href='/set?mode=0' class='card finance " + String(currentDashboard == 0 ? "active" : "") + "'>";
  html += "<div class='card-header'><div class='icon-wrap'>📊</div><span class='badge'>Active</span></div>";
  html += "<h3 class='card-title'>Finance Monitor</h3>";
  html += "<p class='card-desc'>Live cryptocurrency & currency rates with real-time charts and trend analysis.</p>";
  html += "<div class='card-footer'><span class='card-action'>Open Dashboard <span class='arrow'>→</span></span></div>";
  html += "</a>";
  
  // Card 2 - PC Monitor
  html += "<a href='/set?mode=1' class='card pc " + String(currentDashboard == 1 ? "active" : "") + "'>";
  html += "<div class='card-header'><div class='icon-wrap'>🖥️</div><span class='badge'>Active</span></div>";
  html += "<h3 class='card-title'>PC Monitor</h3>";
  html += "<p class='card-desc'>Real-time hardware metrics: CPU, GPU, RAM usage and network statistics.</p>";
  html += "<div class='card-footer'><span class='card-action'>Open Dashboard <span class='arrow'>→</span></span></div>";
  html += "</a>";
  
  // Card 3 - Camera
  html += "<a href='/set?mode=2' class='card camera " + String(currentDashboard == 2 ? "active" : "") + "'>";
  html += "<div class='card-header'><div class='icon-wrap'>📹</div><span class='badge'>Active</span></div>";
  html += "<h3 class='card-title'>Camera Feed</h3>";
  html += "<p class='card-desc'>Live video stream from your security camera with fullscreen display.</p>";
  html += "<div class='card-footer'><span class='card-action'>Open Dashboard <span class='arrow'>→</span></span></div>";
  html += "</a>";
  
  // Card 4 - Cloudflare
  html += "<a href='/set?mode=3' class='card security " + String(currentDashboard == 3 ? "active" : "") + "'>";
  html += "<div class='card-header'><div class='icon-wrap'>🛡️</div><span class='badge'>Active</span></div>";
  html += "<h3 class='card-title'>Cloudflare Security</h3>";
  html += "<p class='card-desc'>Monitor security events, blocked threats and traffic analytics in real-time.</p>";
  html += "<div class='card-footer'><span class='card-action'>Open Dashboard <span class='arrow'>→</span></span></div>";
  html += "</a>";
  
  html += "</div>";
  
  html += "</div>";
  html += "</body></html>";
  
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