#pragma once
#include <Arduino.h>

bool pollPlayback();

void spotifyPlay   ();
void spotifyPause  ();
void spotifyNext   ();
void spotifyPrev   ();
void spotifySeek   (int ms);
void spotifyToggleSave(const String &id, bool save);
