#pragma once
#include <Arduino.h>

void   saveRefreshToken(const String &rt);
String loadRefreshToken();
void   clearRefreshToken();

String buildAuthUrl();
String getRedirectUri();

bool exchangeCode      (const String &code, String &acc, String &ref, int &exp);
bool refreshAccessToken(const String &rt,   String &newAcc, int &exp);
bool ensureToken();

bool runLanAuthFlow();
