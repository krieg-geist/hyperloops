

#ifndef CONFIG_H_
#define CONFIG_H_
#endif

#define LEDPIN         19

#define I2C1_SDA 21
#define I2C1_SCL 22
#define I2C1_SPEED 1000000

#define SD_CS 13
/*
 * DIN MIDI Pinout
 */
#define MIDI_RX_PIN 19
/*
 * You can modify the sample rate as you want
 */
#define SAMPLE_RATE 44100
#define SAMPLE_SIZE_16BIT

#define AUDIBLE_LIMIT   (0.25f/32768.0f)
#define NUM_PLAYERS 8

#define CHANNEL_COUNT   2
#define WORD_SIZE   16
#define I2S1CLK (512*SAMPLE_RATE)
#define BCLK    (SAMPLE_RATE*CHANNEL_COUNT*WORD_SIZE)
#define LRCK    (SAMPLE_RATE*CHANNEL_COUNT)

#define ES8388_ENABLED
#define SAMPLE_BUFFER_SIZE 64

#define ES8388_PIN_MCLK 0
#define ES8388_PIN_SCLK 5
#define ES8388_PIN_LRCK 25
#define ES8388_PIN_DIN  26
#define ES8388_PIN_DOUT 35

// #define I2S_MCLK_PIN ES8388_PIN_MCLK
#define I2S_BCLK_PIN 27
#define I2S_WCLK_PIN 26
#define I2S_DOUT_PIN 25
#define I2S_DIN_PIN -1