#include "touch_handler.h"
#include "config.h"
#include "state.h"
#include "display.h"
#include "spotify_api.h"
#include "auth.h"

#ifndef TOUCH_CS
#define TOUCH_CS 5
#endif

XPT2046_Touchscreen touch(TOUCH_CS);

static uint32_t lastTouchMs = 0;

void initTouch() {
  SPI.begin(14, 33, 13, TOUCH_CS);
  touch.begin();
  touch.setRotation(1);
  Serial.println("[Touch] Initialized");
}

bool inCircle(int tx, int ty, int cx, int cy, int r) {
  int dx = tx - cx, dy = ty - cy;
  return dx*dx + dy*dy <= r*r;
}

void mapTouch(int rawX, int rawY, int &sx, int &sy) {
  // p.x (rawX) drives screen X — inverted (high rawX = left edge)
  // p.y (rawY) drives screen Y — inverted (high rawY = top edge)
  sx = map(rawX, TOUCH_X_MAX, TOUCH_X_MIN, 0, 479);
  sy = map(rawY, TOUCH_Y_MAX, TOUCH_Y_MIN, 0, 319);
  sx = constrain(sx, 0, 479);
  sy = constrain(sy, 0, 319);
}

// Print which zone a touch lands in (for calibration debugging)
static void debugTouchZone(int sx, int sy) {
#if TOUCH_CAL_MODE
  // In cal mode: just print raw->screen, no zone detection
  return;
#endif
  // Print distance to each button center so you can see how close you are
  Serial.printf("[Touch] screen(%d,%d) | dist to: PREV=%d PP=%d NEXT=%d SAVE=%d (radii: SM=%d LG=%d SAVE=%d)\n",
    sx, sy,
    (int)sqrt(sq(sx - BTN_PREV_X) + sq(sy - CTRL_Y)),
    (int)sqrt(sq(sx - BTN_PP_X)   + sq(sy - CTRL_Y)),
    (int)sqrt(sq(sx - BTN_NEXT_X) + sq(sy - CTRL_Y)),
    (int)sqrt(sq(sx - BTN_SAVE_X) + sq(sy - CTRL_Y)),
    BTN_R_SM, BTN_R_LG, BTN_SAVE_R);
}

void handleTouch() {
  if (!touch.touched()) return;

  uint32_t now = millis();
  if (now - lastTouchMs < TOUCH_DEBOUNCE_MS) return;
  lastTouchMs = now;

  TS_Point p = touch.getPoint();
  int sx, sy;
  mapTouch(p.x, p.y, sx, sy);
  Serial.printf("[Touch] raw(%d,%d) -> screen(%d,%d)\n", p.x, p.y, sx, sy);

#if TOUCH_CAL_MODE
  Serial.println("[CAL] Tap corners: TL, TR, BL, BR to find min/max raw values");
  return;   // don't process buttons in cal mode
#endif

  debugTouchZone(sx, sy);

  if (!ensureToken()) {
    Serial.println("[Touch] No valid token, ignoring tap");
    return;
  }

  // ── Previous
  if (inCircle(sx, sy, BTN_PREV_X, CTRL_Y, BTN_R_SM)) {
    Serial.println("[Touch] >> PREVIOUS tapped");
    spotifyPrev();
    delay(400);
    extern uint32_t lastPollMs;
    lastPollMs = 0;
    return;
  }

  // ── Play / Pause 
  if (inCircle(sx, sy, BTN_PP_X, CTRL_Y, BTN_R_LG)) {
    if (curr.playing) {
      Serial.println("[Touch] >> PAUSE tapped");
      spotifyPause();
    } else {
      Serial.println("[Touch] >> PLAY tapped");
      spotifyPlay();
    }
    curr.playing = !curr.playing;
    drawPlayPauseBtn(curr.playing);
    return;
  }

  // ── Next 
  if (inCircle(sx, sy, BTN_NEXT_X, CTRL_Y, BTN_R_SM)) {
    Serial.println("[Touch] >> NEXT tapped");
    spotifyNext();
    delay(400);
    extern uint32_t lastPollMs;
    lastPollMs = 0;
    return;
  }

  // ── Save / Unsave
  if (inCircle(sx, sy, BTN_SAVE_X, CTRL_Y, BTN_SAVE_R) && curr.id.length()) {
    curr.saved = !curr.saved;
    prev.saved = curr.saved;   // keep in sync so poll doesn't clobber the toggle
    Serial.printf("[Touch] >> SAVE tapped -> %s track \"%s\"\n",
                  curr.saved ? "Liking" : "Unliking", curr.title.c_str());
    spotifyToggleSave(curr.id, curr.saved);
    drawSaveBtn(curr.saved);
    return;
  }

  Serial.println("[Touch] No button hit");
}