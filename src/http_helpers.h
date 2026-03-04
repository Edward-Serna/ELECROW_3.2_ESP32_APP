#pragma once
#include <Arduino.h>

bool httpsGet (const String &url, const String &token, String &out, int *statusCode = nullptr);
bool httpsPost(const String &url, const String &ct, const String &body,
               const String &token, String &out, int *statusCode = nullptr);
bool httpsPut (const String &url, const String &token);
