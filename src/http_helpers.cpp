#include "http_helpers.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

bool httpsGet(const String &url, const String &token, String &out, int *st) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, url);
  if (token.length()) h.addHeader("Authorization", "Bearer " + token);
  h.setTimeout(8000);
  h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = h.GET();
  if (st) *st = code;
  out = h.getString();
  h.end();
  return (code == 200 || code == 201);
}

bool httpsPost(const String &url, const String &ct, const String &body,
               const String &token, String &out, int *st) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, url);
  h.addHeader("Content-Type", ct);
  if (token.length()) h.addHeader("Authorization", "Bearer " + token);
  h.setTimeout(10000);
  int code = h.POST((uint8_t *)body.c_str(), body.length());
  if (st) *st = code;
  out = h.getString();
  h.end();
  return (code >= 200 && code < 300);
}

bool httpsPut(const String &url, const String &token) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h; h.begin(c, url);
  h.addHeader("Content-Type", "application/json");
  h.addHeader("Authorization", "Bearer " + token);
  h.addHeader("Content-Length", "0");
  h.setTimeout(8000);
  int code = h.PUT("");
  h.end();
  return (code >= 200 && code < 300);
}
