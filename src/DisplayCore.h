#pragma once
#include "Config.h"
#include <FastLED.h>

void Display_Init();
void Display_Clear();
void Display_Show();
void Display_SetBrightness(uint8_t b);
void Display_Fill(CRGB color);
void Display_DrawPixel(int x, int y, CRGB color);
void Display_DrawDigit(int x, int y, int digit, CRGB color);