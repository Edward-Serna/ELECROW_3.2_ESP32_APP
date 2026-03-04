#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// ── State screens ────────────────────────────────────────────────
void drawStateBooting     (const String &msg = "Starting up...");
void drawStateIdle        ();
void drawStateDisconnected();

// ── Now-playing UI ───────────────────────────────────────────────
void drawHeader    (const String &right = "");
void drawAlbumArt  (const String &url);
void drawTrackInfo (bool force = false);
void drawProgressBar();
void drawPlayPauseBtn(bool playing);
void drawSkipBtn   (int cx, bool isPrev);
void drawSaveBtn   (bool saved);
void drawControls  (bool force = false);
void fullRedraw    ();

// ── Carousel tick (call every loop) ─────────────────────────────
void tickCarousel();

// ── Helpers ──────────────────────────────────────────────────────
String fitString     (const String &s, uint8_t sz, int maxW);
String sanitizeAscii (const String &s);   // UTF-8 decode: transliterates accents, hex-escapes CJK
bool   tft_output    (int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bmp);