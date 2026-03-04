#include "auth.h"
#include "config.h"
#include "state.h"
#include "display.h"
#include "http_helpers.h"
#include "secrets.h"

#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <WiFi.h>

static Preferences prefs;

void saveRefreshToken(const String &rt) {
  prefs.begin("spotify", false);
  prefs.putString("refresh_token", rt);
  prefs.end();
  Serial.println("[Auth] Refresh token saved to NVS");
}
String loadRefreshToken() {
  prefs.begin("spotify", true);
  String rt = prefs.getString("refresh_token", "");
  prefs.end();
  return rt;
}
void clearRefreshToken() {
  prefs.begin("spotify", false);
  prefs.remove("refresh_token");
  prefs.end();
  Serial.println("[Auth] Refresh token cleared");
}

String getRedirectUri() {
#ifdef SPOTIFY_REDIRECT_URI_FIXED
  return String(SPOTIFY_REDIRECT_URI_FIXED);
#else
  return String("http://") + WiFi.localIP().toString() + "/callback";
#endif
}
String buildAuthUrl() {
  String scopes = SPOTIFY_SCOPES;
  scopes.replace(" ", "%20");
  return String(SPOTIFY_AUTH_BASE)
    + "?response_type=code"
    + "&client_id=" + SPOTIFY_CLIENT_ID
    + "&scope=" + scopes
    + "&redirect_uri=" + getRedirectUri();
}

bool exchangeCode(const String &code, String &acc, String &ref, int &exp) {
  String creds = String(SPOTIFY_CLIENT_ID) + ":" + SPOTIFY_CLIENT_SECRET;
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, SPOTIFY_TOKEN_URL);
  h.addHeader("Authorization", "Basic " + base64::encode(creds));
  h.addHeader("Content-Type", "application/x-www-form-urlencoded");
  h.setTimeout(10000);
  String body = "grant_type=authorization_code&code=" + code + "&redirect_uri=" + getRedirectUri();
  int rc = h.POST((uint8_t *)body.c_str(), body.length());
  String resp = h.getString(); h.end();
  if (rc != 200) { Serial.printf("[Auth] exchange failed %d: %s\n", rc, resp.c_str()); return false; }
  JsonDocument doc; deserializeJson(doc, resp);
  acc = doc["access_token"].as<String>();
  ref = doc["refresh_token"].as<String>();
  exp = doc["expires_in"] | 3600;
  Serial.println("[Auth] Code exchanged successfully");
  return acc.length() > 0;
}

bool refreshAccessToken(const String &rt, String &newAcc, int &exp) {
  String creds = String(SPOTIFY_CLIENT_ID) + ":" + SPOTIFY_CLIENT_SECRET;
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, SPOTIFY_TOKEN_URL);
  h.addHeader("Authorization", "Basic " + base64::encode(creds));
  h.addHeader("Content-Type", "application/x-www-form-urlencoded");
  h.setTimeout(10000);
  String body = "grant_type=refresh_token&refresh_token=" + rt;
  int code = h.POST((uint8_t *)body.c_str(), body.length());
  String resp = h.getString(); h.end();
  if (code != 200) { Serial.printf("[Auth] refresh failed %d\n", code); return false; }
  JsonDocument doc; deserializeJson(doc, resp);
  newAcc = doc["access_token"].as<String>();
  exp    = doc["expires_in"] | 3600;
  String newRt = doc["refresh_token"] | "";
  if (newRt.length()) saveRefreshToken(newRt);
  Serial.println("[Auth] Access token refreshed");
  return newAcc.length() > 0;
}

bool ensureToken() {
  if (g_accessToken.length() && millis() < g_tokenExpiresAt - 60000) return true;
  String rt = loadRefreshToken();
  if (!rt.length()) return false;
  String newAcc; int exp;
  if (!refreshAccessToken(rt, newAcc, exp)) return false;
  g_accessToken    = newAcc;
  g_tokenExpiresAt = millis() + (uint32_t)exp * 1000;
  return true;
}

static WebServer     portalServer(80);
static volatile bool g_codeReceived = false;
static String        g_authCode;

bool runLanAuthFlow() {
  portalServer.on("/", HTTP_GET, []() {
    String authUrl = buildAuthUrl();
    String ip = WiFi.localIP().toString();
    String html =
      "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>body{font-family:sans-serif;background:#000;color:#fff;display:flex;flex-direction:column;"
      "align-items:center;justify-content:center;min-height:100vh;margin:0;padding:20px;}"
      "h2{color:#1DB954;}a{display:inline-block;background:#1DB954;color:#000;padding:14px 28px;"
      "border-radius:28px;text-decoration:none;font-weight:700;margin-top:14px;}"
      "code{color:#1DB954;}</style></head><body>"
      "<h2>Spotify Setup</h2>"
      "<p>ESP32 IP: <code>" + ip + "</code></p>"
      "<a href='" + authUrl + "'>Authorize with Spotify</a>"
      "<p style='font-size:12px'>Redirect URI: <code>" + getRedirectUri() + "</code></p>"
      "</body></html>";
    portalServer.send(200, "text/html", html);
  });
  portalServer.on("/callback", HTTP_GET, []() {
    if (portalServer.hasArg("code")) {
      g_authCode     = portalServer.arg("code");
      g_codeReceived = true;
      portalServer.send(200, "text/html",
        "<html><body style='font-family:sans-serif;background:#000;color:#1DB954;"
        "display:flex;align-items:center;justify-content:center;min-height:100vh;text-align:center'>"
        "<div><h2>Done!</h2><p style='color:#aaa'>Close this page.</p></div></body></html>");
    } else {
      portalServer.send(400, "text/plain", "Missing code");
    }
  });
  portalServer.onNotFound([]() { portalServer.send(404, "text/plain", "Not found"); });
  portalServer.begin();

  drawStateBooting("Waiting for auth...");
  Serial.printf("[Auth] Visit http://%s to authorize\n", WiFi.localIP().toString().c_str());

  uint32_t t0 = millis();
  while (!g_codeReceived) {
    portalServer.handleClient();
    if (millis() - t0 > 180000) {
      Serial.println("[Auth] Timeout waiting for code");
      portalServer.stop(); return false;
    }
    delay(10);
  }
  portalServer.stop();
  Serial.println("[Auth] Code received, exchanging...");

  String acc, ref; int exp = 0;
  if (!exchangeCode(g_authCode, acc, ref, exp)) { delay(5000); return false; }
  saveRefreshToken(ref);
  g_accessToken    = acc;
  g_tokenExpiresAt = millis() + (uint32_t)exp * 1000;
  return true;
}
