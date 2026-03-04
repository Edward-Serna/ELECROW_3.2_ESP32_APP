#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

#include "config.h"
#include "state.h"
#include "display.h"
#include "auth.h"
#include "spotify_api.h"
#include "touch_handler.h"
#include "secrets.h"

// Global state definitions
Track    curr, prev;
String   g_accessToken;
uint32_t g_tokenExpiresAt = 0;
String   currAlbumArtUrl;
bool     needsFullRedraw  = true;
AppState appState         = STATE_BOOTING;

// Carousel state
int      carouselTitleOffset  = 0;
int      carouselArtistOffset = 0;
uint32_t carouselLastStep     = 0;
uint32_t carouselPauseUntil   = 0;

// Polling timers (touch_handler.cpp references lastPollMs via extern)
uint32_t lastPollMs  = 0;
uint32_t lastTickMs  = 0;

static bool connectHomeWifi(uint32_t timeout = 20000) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(200);
  Serial.printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeout) {
    delay(300);
    Serial.printf("[WiFi] status=%d\n", WiFi.status());
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected! IP=%s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("[WiFi] Failed to connect");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n[Main] Booting Spotify Controller");

  tft.begin();
  tft.setRotation(1);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

  TJpgDec.setCallback(tft_output);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);

  initTouch();

  // ── Boot screen
  appState = STATE_BOOTING;
  drawStateBooting("Connecting to WiFi...");

  if (!connectHomeWifi()) {
    appState = STATE_DISCONNECTED;
    drawStateDisconnected();
    delay(4000); 
    ESP.restart(); 
    return;
  }

  drawStateBooting("WiFi connected!");
  delay(400);

  // ── Auth
  String rt = loadRefreshToken();
  if (!rt.length()) {
    drawStateBooting("Waiting for Spotify auth...");
    if (!runLanAuthFlow()) { 
      delay(2000); 
      ESP.restart(); return; 
    }
  }

  if (!ensureToken()) {
    Serial.println("[Main] Token error -> clearing and re-auth");
    drawStateBooting("Token error, re-authorizing...");
    clearRefreshToken(); 
    delay(2000);
    ESP.restart(); return;
  }

  Serial.println("[Main] Ready!");
  needsFullRedraw = true;
  lastPollMs      = 0;  // poll immediately
}

void loop() {
  uint32_t now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (appState != STATE_DISCONNECTED) {
      appState = STATE_DISCONNECTED;
      drawStateDisconnected();
      Serial.println("[Main] WiFi lost, attempting reconnect...");
    }
    WiFi.reconnect();
    delay(500);
    return;
  }

  handleTouch();

  // ── Progress tick (smooth bar)
  if (curr.active && curr.playing && now - lastTickMs >= TICK_INTERVAL_MS) {
    lastTickMs = now;
    curr.progress_ms = min(curr.progress_ms + (int32_t)TICK_INTERVAL_MS, curr.duration_ms);
    drawProgressBar();
  }

  // ── Carousel tick
  tickCarousel();

  // ── Spotify poll
  if (now - lastPollMs >= POLL_INTERVAL_MS || needsFullRedraw) {
    lastPollMs = now;

    if (!ensureToken()) {
      Serial.println("[Main] ensureToken failed");
      return;
    }

    Track before = curr;
    if (pollPlayback()) {
      bool trackChanged  = (curr.id     != before.id);
      bool activeChanged = (curr.active != prev.active);

      // Determine new app state
      if (!curr.active) {
        if (appState != STATE_IDLE) {
          appState = STATE_IDLE;
          drawStateIdle();
          Serial.println("[Main] -> STATE_IDLE");
        }
      } else if (curr.playing) {
        AppState next = STATE_PLAYING;
        if (appState != next || trackChanged || needsFullRedraw) {
          appState = next;
          fullRedraw();
          Serial.printf("[Main] -> STATE_PLAYING: %s - %s\n",
                        curr.title.c_str(), curr.artist.c_str());
        } else {
          drawTrackInfo(false);
          drawProgressBar();
          drawControls(false);
        }
      } else {
        AppState next = STATE_PAUSED;
        if (appState != next || trackChanged || needsFullRedraw) {
          appState = next;
          fullRedraw();
          Serial.printf("[Main] -> STATE_PAUSED: %s - %s\n",
                        curr.title.c_str(), curr.artist.c_str());
        } else {
          drawTrackInfo(false);
          drawProgressBar();
          drawControls(false);
        }
      }

      if (trackChanged) {
        carouselTitleOffset  = 0;
        carouselArtistOffset = 0;
        carouselPauseUntil   = millis() + CAROUSEL_DELAY_MS;
      }

      prev = curr;
    }
  }

  delay(20);
}
