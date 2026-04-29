#include "DisplayCore.h"

CRGB leds[NUM_LEDS];

static const uint8_t digits[11][5] = {
    {0x07, 0x05, 0x05, 0x05, 0x07}, {0x01, 0x01, 0x01, 0x01, 0x01}, {0x07, 0x01, 0x07, 0x04, 0x07}, {0x07, 0x01, 0x07, 0x01, 0x07}, {0x05, 0x05, 0x07, 0x01, 0x01}, {0x07, 0x04, 0x07, 0x01, 0x07}, {0x07, 0x04, 0x07, 0x05, 0x07}, {0x07, 0x01, 0x01, 0x01, 0x01}, {0x07, 0x05, 0x07, 0x05, 0x07}, {0x07, 0x05, 0x07, 0x01, 0x07}, {0x00, 0x00, 0x07, 0x00, 0x00} // 10: "-"
};

uint16_t getIndex(uint8_t x, uint8_t y)
{
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
        return 0;
    return (x < 8) ? (y * 8 + x) : (64 + y * 8 + (x - 8));
}

void Display_Init()
{
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    Display_Clear();
}

void Display_Clear() { FastLED.clear(); }
void Display_Show() { FastLED.show(); }
void Display_SetBrightness(uint8_t b) { FastLED.setBrightness(b); }
void Display_Fill(CRGB color) { fill_solid(leds, NUM_LEDS, color); }

void Display_DrawPixel(int x, int y, CRGB color)
{
    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT)
        leds[getIndex(x, y)] = color;
}

void Display_DrawDigit(int x, int y, int digit, CRGB color)
{
    if (digit < 0 || digit > 10)
        return;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (digits[digit][r] & (1 << (2 - c)))
                Display_DrawPixel(x + c, y + r, color);
}