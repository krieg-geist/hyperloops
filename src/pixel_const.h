#include <Arduino.h>

// I needed because I wired my neopixel rings weirdly, you might not
const uint8_t led_mapping[16] =
    {8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9};

const uint32_t colourLUT[10] {
    0x001E0F00,
    0x00FF0A0A, // Red active
    0x000A1E1E,
    0x000AFF0A, // Green active
    0x002E0A1E,
    0x000AF0FF, // Blue active
    0x00000000,
    0x0000FF0A, // Div
    0x00202020, // Active no note
    0x00FFFFFF  // Active note
};

const uint32_t volLUT[16] {
    0x00006000,
    0x00007000,
    0x00008000,
    0x00009000,
    0x0000AA00,
    0x0000BB00,
    0x0000CC00,
    0x0000DD00,
    0x0000EE00,
    0x0000FF00,
    0x00FFFF00,
    0x00FFFF00,
    0x00FFFF00,
    0x00FF9000,
    0x00FF9000,
    0x00FF0000,
};