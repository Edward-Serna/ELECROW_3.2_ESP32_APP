#pragma once
#include <Arduino.h>

enum AppState {
  STATE_BOOTING,
  STATE_IDLE,        
  STATE_PLAYING,
  STATE_PAUSED,
  STATE_DISCONNECTED
};

struct Track {
  String  id, title, artist, album;
  int32_t duration_ms  = 0;
  int32_t progress_ms  = 0;
  bool    playing      = false;
  bool    saved        = false;
  bool    active       = false;
};

// Global state (defined in main.cpp)
extern Track     curr, prev;
extern String    g_accessToken;
extern uint32_t  g_tokenExpiresAt;
extern String    currAlbumArtUrl;
extern bool      needsFullRedraw;
extern AppState  appState;

// carousel offsets
extern int  carouselTitleOffset;
extern int  carouselArtistOffset;
extern uint32_t carouselLastStep;
extern uint32_t carouselPauseUntil;
