#include "spotify_api.h"
#include "config.h"
#include "state.h"
#include "http_helpers.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Transport controls
void spotifyPlay()  {
  bool ok = httpsPut(String(SPOTIFY_API_BASE) + "/me/player/play",  g_accessToken);
  Serial.printf("[Spotify] Play  -> %s\n", ok ? "OK" : "FAIL");
}
void spotifyPause() {
  bool ok = httpsPut(String(SPOTIFY_API_BASE) + "/me/player/pause", g_accessToken);
  Serial.printf("[Spotify] Pause -> %s\n", ok ? "OK" : "FAIL");
}
void spotifySeek(int ms) {
  bool ok = httpsPut(String(SPOTIFY_API_BASE) + "/me/player/seek?position_ms=" + ms, g_accessToken);
  Serial.printf("[Spotify] Seek %dms -> %s\n", ms, ok ? "OK" : "FAIL");
}

static void spotifyPost(const String &path) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, String(SPOTIFY_API_BASE) + path);
  h.addHeader("Authorization", "Bearer " + g_accessToken);
  h.addHeader("Content-Length", "0");
  h.setTimeout(8000);
  int code = h.POST(""); h.end();
  Serial.printf("[Spotify] POST %s -> %d\n", path.c_str(), code);
}
void spotifyNext() { spotifyPost("/me/player/next");     }
void spotifyPrev() { spotifyPost("/me/player/previous"); }

void spotifyToggleSave(const String &id, bool save) {
  String url  = String(SPOTIFY_API_BASE) + "/me/tracks";
  String body = "{\"ids\":[\"" + id + "\"]}";
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, url);
  h.addHeader("Authorization", "Bearer " + g_accessToken);
  h.addHeader("Content-Type", "application/json");
  h.setTimeout(8000);
  int code;
  if (save) code = h.PUT((uint8_t *)body.c_str(), body.length());
  else      code = h.sendRequest("DELETE", (uint8_t *)body.c_str(), body.length());
  h.end();
  Serial.printf("[Spotify] %s track %s -> %d\n", save ? "Saved" : "Removed", id.c_str(), code);
}

bool pollPlayback() {
  String resp; int status = 0;
  bool ok = httpsGet(String(SPOTIFY_API_BASE) + "/me/player", g_accessToken, resp, &status);
  Serial.printf("[Spotify] Poll -> HTTP %d (%d bytes)\n", status, resp.length());

  if (status == 204) { curr.active = false; return true; }
  if (!ok)           { return false; }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) { Serial.println("[Spotify] JSON parse error"); return false; }

  curr.active      = true;
  curr.playing     = doc["is_playing"] | false;
  curr.progress_ms = doc["progress_ms"] | 0;

  JsonObject item = doc["item"];
  if (item.isNull()) { curr.active = false; return true; }

  String newId = item["id"].as<String>();
  bool changed = (newId != curr.id);
  curr.id          = newId;
  curr.duration_ms = item["duration_ms"] | 0;
  curr.title       = item["name"].as<String>();
  curr.album       = item["album"]["name"].as<String>();

  // Best album art close to target size
  JsonArray imgs = item["album"]["images"].as<JsonArray>();
  if (!imgs.isNull() && imgs.size() > 0) {
    String best = imgs[0]["url"] | "";
    for (JsonObject img : imgs) {
      int w = img["width"] | 0;
      if (w >= ART_W && w < (int)(imgs[0]["width"] | 9999)) best = img["url"] | best;
    }
    if (best.length()) currAlbumArtUrl = best;
  }

  String artists;
  for (JsonObject a : item["artists"].as<JsonArray>()) {
    if (artists.length()) artists += ", ";
    artists += a["name"].as<String>();
  }
  curr.artist = artists;

  if (changed && curr.id.length()) {
    Serial.printf("[Spotify] Track changed -> \"%s\" by %s\n", curr.title.c_str(), curr.artist.c_str());
    String sr;
    if (httpsGet(String(SPOTIFY_API_BASE) + "/me/tracks/contains?ids=" + curr.id, g_accessToken, sr)) {
      JsonDocument sd; deserializeJson(sd, sr);
      curr.saved = sd[0] | false;
      Serial.printf("[Spotify] Track saved: %s\n", curr.saved ? "yes" : "no");
    }
  } else {
    curr.saved = prev.saved;
  }
  return true;
}