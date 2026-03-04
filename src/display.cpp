#include "display.h"
#include "config.h"
#include "state.h"
#include <TJpg_Decoder.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFi.h>

TFT_eSPI tft;

// JPEG callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bmp) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bmp);
  return 1;
}

// Helpers
String fitString(const String &s, uint8_t sz, int maxW) {
  tft.setTextSize(sz);
  if (tft.textWidth(s) <= maxW) return s;
  String out = s;
  while (out.length() > 1 && tft.textWidth(out + "...") > maxW)
    out.remove(out.length() - 1);
  return out + "...";
}

// Unicode helpers
// TFT_eSPI bitmap fonts only cover ASCII 32-127. Rather than replacing
// every non-ASCII char with '?', we decode UTF-8 codepoints and do
// best-effort transliteration for common Latin-extended, accented, and
// symbol ranges. CJK / Hangul / Kana that have no ASCII equivalent are
// replaced with the Unicode replacement char displayed as its hex escape,
// e.g. <4EBA> — still ugly but at least tells you SOMETHING is there.
// For full native CJK rendering you'd need a .vlw smooth font loaded from
// SPIFFS (see TFT_eSPI smooth font docs).
// Decode one UTF-8 sequence starting at s[i], advance i, return codepoint.
static uint32_t nextCodepoint(const String &s, size_t &i) {
  uint8_t c = (uint8_t)s[i];
  if (c < 0x80)  { i++; return c; }
  uint32_t cp; int extra;
  if      ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
  else { i++; return 0xFFFD; }
  i++;
  for (int j = 0; j < extra && i < s.length(); j++, i++)
    cp = (cp << 6) | ((uint8_t)s[i] & 0x3F);
  return cp;
}

// Best-effort codepoint → ASCII string (nullptr = no mapping, use hex fallback)
static const char* transliterate(uint32_t cp) {
  // Latin-1 supplement (0x00C0-0x00FF) — accented Latin
  switch (cp) {
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
    case 0x00C4: case 0x00C5: return "A";
    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3:
    case 0x00E4: case 0x00E5: return "a";
    case 0x00C6: return "AE";  case 0x00E6: return "ae";
    case 0x00C7: return "C";   case 0x00E7: return "c";
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: return "E";
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: return "e";
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: return "I";
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: return "i";
    case 0x00D0: return "D";   case 0x00F0: return "d";
    case 0x00D1: return "N";   case 0x00F1: return "n";
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
    case 0x00D6: case 0x00D8: return "O";
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5:
    case 0x00F6: case 0x00F8: return "o";
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: return "U";
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: return "u";
    case 0x00DD: return "Y";   case 0x00FD: case 0x00FF: return "y";
    case 0x00DE: return "Th";  case 0x00FE: return "th";
    case 0x00DF: return "ss";
    // Latin Extended-A (0x0100-0x017F) common ones
    case 0x0141: return "L";   case 0x0142: return "l";
    case 0x0152: return "OE";  case 0x0153: return "oe";
    case 0x0160: return "S";   case 0x0161: return "s";
    case 0x017D: return "Z";   case 0x017E: return "z";
    // Punctuation / symbols that have ASCII equivalents
    case 0x2018: case 0x2019: return "'";
    case 0x201C: case 0x201D: return "-";
    case 0x2013: return "-";
    case 0x2014: return "--";
    case 0x2026: return "...";
    case 0x00B7: case 0x2022: return "-";
    case 0x00A9: return "(c)";
    case 0x00AE: return "(R)";
    case 0x2122: return "TM";
    case 0x00D7: return "x";
    case 0x266A: case 0x266B: return "~";  // musical notes
    default: return nullptr;
  }
}

String sanitizeAscii(const String &s) {
  String out;
  out.reserve(s.length());
  size_t i = 0;
  while (i < s.length()) {
    uint32_t cp = nextCodepoint(s, i);
    if (cp < 0x80) {
      if (cp >= 0x20) out += (char)cp;
    } else {
      const char *t = transliterate(cp);
      if (t) {
        out += t;
      } else {
        // No mapping — show as <XXXX> so the user knows something is there
        // rather than a silent '?'
        char buf[8];
        if (cp <= 0xFFFF)  snprintf(buf, sizeof(buf), "<%04X>", (unsigned)cp);
        else               snprintf(buf, sizeof(buf), "<%05X>", (unsigned)cp);
        out += buf;
      }
    }
  }
  return out;
}

void drawStateBooting(const String &msg) {
  tft.fillScreen(C_BG);
  drawHeader("BOOT");
  tft.setTextColor(C_GREEN, C_BG); tft.setTextDatum(MC_DATUM); tft.setTextSize(3);
  tft.drawString("Elecrow Spotify Player", 240, 130);
  tft.setTextColor(C_GRAY, C_BG);  tft.setTextSize(2);
  tft.drawString(msg, 240, 175);
  tft.setTextColor(C_MUTED, C_BG); tft.setTextSize(2);
  tft.drawString(WiFi.localIP().toString(), 240, 205);
}

void drawStateIdle() {
  tft.fillScreen(C_BG);
  drawHeader("IDLE");
  tft.setTextColor(C_GRAY, C_BG);   tft.setTextDatum(MC_DATUM); tft.setTextSize(2);
  tft.drawString("No active playback", 240, 150);
  tft.setTextColor(C_MUTED, C_BG);  tft.setTextSize(2);
  tft.drawString("Play something on Spotify", 240, 175);
}

void drawStateDisconnected() {
  tft.fillScreen(C_BG);
  drawHeader("NO WIFI");
  tft.setTextColor(C_RED, C_BG);    tft.setTextDatum(MC_DATUM); tft.setTextSize(2);
  tft.drawString("WiFi Disconnected", 240, 140);
  tft.setTextColor(C_GRAY, C_BG);   tft.setTextSize(2);
  tft.drawString("Reconnecting...", 240, 170);
}

void drawHeader(const String &right) {
  tft.fillRect(0, HDR_Y, 480, HDR_H, C_GREEN);
  tft.setTextColor(TFT_BLACK, C_GREEN);
  tft.setTextDatum(ML_DATUM); tft.setTextSize(2);
  tft.drawString("Booting...", 12, HDR_Y + HDR_H / 2);
  if (right.length()) {
    tft.setTextDatum(MR_DATUM); tft.setTextSize(1);
    tft.drawString(right, 470, HDR_Y + HDR_H / 2);
  }
}

void drawAlbumArt(const String &url) {
  if (!url.length()) { tft.fillRect(ART_X, ART_Y, ART_W, ART_H, C_BG); return; }

  // tft.fillRect(ART_X, ART_Y, ART_W, ART_H, C_MUTED);
  // tft.setTextColor(C_GRAY, C_MUTED); tft.setTextDatum(MC_DATUM); tft.setTextSize(1);
  // tft.drawString("...", ART_X + ART_W/2, ART_Y + ART_H/2);

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) { Serial.println("[Art] begin failed"); return; }
  http.setTimeout(10000);
  int code = http.GET();
  if (code != 200) { Serial.printf("[Art] HTTP %d\n", code); http.end(); return; }

  int len = http.getSize();
  if (len <= 0 || len > 150000) { Serial.printf("[Art] bad size %d\n", len); http.end(); return; }

  uint8_t *jpg = (uint8_t *)malloc(len);
  if (!jpg) { Serial.println("[Art] malloc failed"); http.end(); return; }

  WiFiClient *s = http.getStreamPtr();
  int total = 0;
  uint32_t t0 = millis();
  while (total < len && millis() - t0 < 8000) {
    int av = s->available();
    if (av > 0) { int n = s->readBytes(jpg + total, min(av, len - total)); if (n > 0) total += n; }
    else delay(1);
  }
  http.end();

  if (total < len) { Serial.printf("[Art] incomplete %d/%d\n", total, len); free(jpg); return; }

  TJpgDec.setJpgScale(2);
  TJpgDec.setSwapBytes(true);
  JRESULT r = TJpgDec.drawJpg(ART_X, ART_Y, jpg, total);
  if (r != JDR_OK) Serial.printf("[Art] decode err %d\n", (int)r);
  free(jpg);
}

static int drawCarouselLabel(const String &text, uint8_t sz, int x, int y, int clipW, int offset, uint16_t color) {
  tft.setTextSize(sz);
  int fullW = tft.textWidth(text);
  int lineH = sz * 8 + 4;
  tft.fillRect(x, y, clipW, lineH, C_BG);
  tft.setTextColor(color, C_BG);
  tft.setTextDatum(TL_DATUM);
  if (fullW <= clipW) {
    // Fits — draw normally at screen coords
    tft.drawString(text, x, y);
    return 0;
  }
  // Scrolling — setViewport makes (0,0) the viewport's top-left corner
  tft.setViewport(x, y, clipW, lineH);
  tft.drawString(text, -offset, 0);
  tft.resetViewport();
  return fullW;
}

void tickCarousel() {
  uint32_t now = millis();
  if (now < carouselPauseUntil) return;
  if (now - carouselLastStep < CAROUSEL_SPEED_MS) return;
  carouselLastStep = now;

  tft.setTextSize(3);
  int titleW  = tft.textWidth(sanitizeAscii(curr.title));
  int artistW = tft.textWidth(sanitizeAscii(curr.artist));

  bool titleScrolls  = titleW  > INFO_W;
  bool artistScrolls = artistW > INFO_W;

  if (titleScrolls) {
    carouselTitleOffset++;
    if (carouselTitleOffset > titleW + 30) {
      carouselTitleOffset = 0;
      carouselPauseUntil = now + CAROUSEL_DELAY_MS;
    }
    drawCarouselLabel(sanitizeAscii(curr.title),  3, INFO_X, INFO_Y + 10,  INFO_W, carouselTitleOffset, C_WHITE);
  }
  if (artistScrolls) {
    carouselArtistOffset++;
    if (carouselArtistOffset > artistW + 30) {
      carouselArtistOffset = 0;
      carouselPauseUntil = now + CAROUSEL_DELAY_MS;
    }
    drawCarouselLabel(sanitizeAscii(curr.artist), 3, INFO_X, INFO_Y + 55,  INFO_W, carouselArtistOffset, C_GRAY);
  }
}

void drawTrackInfo(bool force) {
  tft.setTextDatum(TL_DATUM);

  // Sanitize before every draw — foreign chars render as garbage on default font
  String title  = sanitizeAscii(curr.title.length()  ? curr.title  : "---");
  String artist = sanitizeAscii(curr.artist.length() ? curr.artist : "No device active");
  String album  = sanitizeAscii(curr.album);

  if (force || curr.title != prev.title) {
    carouselTitleOffset = 0;
    carouselPauseUntil  = millis() + CAROUSEL_DELAY_MS;
    drawCarouselLabel(title, 3, INFO_X, INFO_Y + 10, INFO_W, 0, C_WHITE);
  }
  if (force || curr.artist != prev.artist) {
    carouselArtistOffset = 0;
    drawCarouselLabel(artist, 3, INFO_X, INFO_Y + 55, INFO_W, 0, C_GRAY);
  }
  if (force || curr.album != prev.album) {
    drawCarouselLabel(album, 2, INFO_X, INFO_Y + 100, INFO_W, 0, C_MUTED);
  }
  if (force || curr.saved != prev.saved) {
    drawSaveBtn(curr.saved);
  }
}

void drawProgressBar() {
  tft.fillRoundRect(PB_X, PB_Y, PB_W, PB_H, 3, C_BORDER);
  if (curr.duration_ms > 0) {
    int fill = constrain((int)((float)curr.progress_ms / curr.duration_ms * PB_W), 0, PB_W);
    if (fill > 0) tft.fillRoundRect(PB_X, PB_Y, fill, PB_H, 3, C_GREEN);
  }
  tft.fillRect(PB_X,            TIME_Y + 4, 55, 12, C_BG);
  tft.fillRect(PB_X + PB_W - 55, TIME_Y + 4, 55, 12, C_BG);
  tft.setTextColor(C_GRAY, C_BG); 
  tft.setTextSize(2);
  char buf[8];
  int s = curr.progress_ms / 1000;
  snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
  tft.setTextDatum(TL_DATUM); tft.drawString(buf, PB_X, TIME_Y + 5);
  s = curr.duration_ms / 1000;
  snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
  tft.setTextDatum(TR_DATUM); tft.drawString(buf, PB_X + PB_W, TIME_Y + 5);
}

void drawPlayPauseBtn(bool playing) {
  tft.fillCircle(BTN_PP_X, CTRL_Y, BTN_R_LG, C_GREEN);
  if (playing) {
    tft.fillRect(BTN_PP_X - 10, CTRL_Y - 11, 7, 22, TFT_BLACK);
    tft.fillRect(BTN_PP_X + 3,  CTRL_Y - 11, 7, 22, TFT_BLACK);
  } else {
    for (int i = 0; i < 16; i++) {
      int h = 2 * (16 - i);
      tft.drawFastVLine(BTN_PP_X - 7 + i, CTRL_Y - h / 2, h, TFT_BLACK);
    }
  }
}

void drawSkipBtn(int cx, bool isPrev) {
  tft.fillCircle(cx, CTRL_Y, BTN_R_SM, C_CARD);
  tft.drawCircle(cx, CTRL_Y, BTN_R_SM, C_BORDER);
  int dir = isPrev ? -1 : 1;
  for (int i = 0; i < 13; i++) {
    int h = 2 * (13 - i);
    tft.drawFastVLine(cx + dir * (-5 + i), CTRL_Y - h / 2, h, C_WHITE);
  }
  tft.fillRect(cx + dir * 7, CTRL_Y - 11, 4, 22, C_WHITE);
}

void drawSaveBtn(bool saved) {
  uint16_t bg    = saved ? C_GREEN : C_CARD;  
  uint16_t fg    = saved ? TFT_BLACK : C_GRAY;
  tft.fillCircle(BTN_SAVE_X, CTRL_Y + 2, BTN_SAVE_R, bg);
  tft.drawCircle(BTN_SAVE_X, CTRL_Y + 2, BTN_SAVE_R, C_BORDER);
  // simple heart: two circles + triangle
  tft.fillCircle(BTN_SAVE_X - 5, CTRL_Y - 2, 5, fg);
  tft.fillCircle(BTN_SAVE_X + 5, CTRL_Y - 2, 5, fg);
  // fill triangle beneath
  for (int row = 0; row < 11; row++) {
    int hw = 10 - row;
    tft.drawFastHLine(BTN_SAVE_X - hw, CTRL_Y + row + 1, hw * 2 + 1, fg);
  }
}

void drawControls(bool force) {
  if (force) {
    tft.fillRect(0, CTRL_Y - BTN_R_LG - 4, 480, BTN_R_LG * 2 + 8, C_BG);
    drawSkipBtn(BTN_PREV_X, true);
    drawSkipBtn(BTN_NEXT_X, false);
    drawSaveBtn(curr.saved);
  }
  if (force || curr.playing != prev.playing) drawPlayPauseBtn(curr.playing);
  if (force || curr.saved   != prev.saved)   drawSaveBtn(curr.saved);
}

void fullRedraw() {
  tft.fillScreen(C_BG);
  // drawHeader(curr.playing ? "PLAYING" : "PAUSED");
  drawAlbumArt(currAlbumArtUrl);
  drawTrackInfo(true);
  drawProgressBar();
  drawControls(true);
  needsFullRedraw = false;
}