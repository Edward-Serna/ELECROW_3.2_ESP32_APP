#pragma once
#include <Arduino.h>

// Display layout
#define HDR_Y   0
#define HDR_H   34
#define ART_W   100
#define ART_H   100
#define ART_X   18
#define ART_Y   35
#define INFO_X  200
#define INFO_Y  55
#define INFO_W  250
#define PB_X    8
#define PB_Y    215
#define PB_W    464
#define PB_H    6
#define TIME_Y  230
#define CTRL_Y  285

// Button layout 
#define BTN_PREV_X  140
#define BTN_PP_X    240
#define BTN_NEXT_X  340
#define BTN_SAVE_X  450
#define BTN_R_SM    30
#define BTN_R_LG    32
#define BTN_SAVE_R  22

// Colors (RGB565)
#define C_BG     0x0000
#define C_CARD   0x18C3
#define C_GREEN  0x06C9
#define C_WHITE  0xFFFF
#define C_GRAY   0x8C71
#define C_MUTED  0x4208
#define C_BORDER 0x2104
#define C_RED    0xF800
#define C_YELLOW 0xFFE0

// Touchscreen config─
#define TOUCH_X_MIN   150   // rawX at screen right edge (sx=479) — inverted
#define TOUCH_X_MAX  3880   // rawX at screen left  edge (sx=0)   — inverted
#define TOUCH_Y_MIN   320   // rawY at screen bottom     (sy=319)  — inverted
#define TOUCH_Y_MAX  3880   // rawY at screen top        (sy=0)    — inverted
#define TOUCH_DEBOUNCE_MS 350

#define TOUCH_CAL_MODE 0

// Polling / tick intervals─
#define POLL_INTERVAL_MS   3000
#define TICK_INTERVAL_MS   500

// Spotify URLs───
static const char *SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token";
static const char *SPOTIFY_API_BASE  = "https://api.spotify.com/v1";
static const char *SPOTIFY_AUTH_BASE = "https://accounts.spotify.com/authorize";
static const char *SPOTIFY_SCOPES    = "user-read-playback-state user-modify-playback-state user-library-read user-library-modify";

// Carousel Config
#define CAROUSEL_DELAY_MS   500
#define CAROUSEL_SPEED_MS   30 