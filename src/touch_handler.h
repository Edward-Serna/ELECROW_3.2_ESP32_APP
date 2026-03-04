#pragma once
#include <Arduino.h>
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen touch;

void initTouch();
void handleTouch();

void mapTouch(int rawX, int rawY, int &sx, int &sy);
bool inCircle(int tx, int ty, int cx, int cy, int r);
