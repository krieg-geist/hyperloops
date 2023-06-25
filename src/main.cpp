#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_I2CDevice.h>
#include <Rotary.h>
#include <RotaryEncOverMCP.h>
#include <WiFi.h>
#include <Wire.h>
#include <MIDI.h>

#include "config.h"
#include "Hardware.h"
#include "Multiplexer.h"
#include "Button.h"
#include "euclid.h"
#include "pixel_const.h"
#include "i2s_interface.h"
#include "player.h"
#include "patch_manager.h"
#include "es8388.h"
#include "delay.h"
#include "ml_reverb.h"

unsigned long newTime;
unsigned long oldTime;
unsigned long INTERVAL; //micros

TwoWire I2C1 = TwoWire(0); //I2C1 bus
TwoWire I2C2 = TwoWire(1); //I2C2 bus

TaskHandle_t Core0TaskHnd;

// MIDI stuff
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, serialMidi);

#ifndef absf
#define absf(a) ((a >= 0.0f) ? (a) : (-a))
#endif

static float fl_sample[SAMPLE_BUFFER_SIZE];
static float fr_sample[SAMPLE_BUFFER_SIZE];

inline void audio_task()
{
  memset(fl_sample, 0, sizeof(fl_sample));
  memset(fr_sample, 0, sizeof(fr_sample));

  playerProcess(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);
  // Delay_Process_Buff(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);
  Reverb_Process(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);

  /* function blocks and returns when sample is put into buffer */
  if (i2s_write_stereo_samples_buff(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE))
  {
    ; /* nothing for here */
  }
}

#define DEBUG

#define RINGS 4
#define RINGPIX 16

#define NUMPIXELS RINGS *RINGPIX // Popular NeoPixel ring size

byte numModes = 3;
/*
  0 = note place,
  1 = length edit,
  2 = offset edit,
*/

const uint8_t divRatio[5] = {16, 8, 4, 2, 1}; //note multipliers = "1","2","4","8","16","32"

int channel_settings = -1;
uint8_t reverbLevel = 3;

struct ringState
{
  byte mode;
  int8_t activeNote;
  byte loopLength;
  byte numNotes;
  byte offset;
  int8_t clkDiv;
  uint16_t userNotes;
  uint16_t euclidNotes;
  int8_t panValue;
  int8_t vol;
};

ringState ringStates[RINGS];
bool buttonState[RINGS]; // black buttons below

// encoder state stuff
unsigned long lastTransition[RINGS];

// button state stuff

/* function prototypes */

void pollMCP();
void initButtons();
void initRotaryEncoders();
int getEncoderSpeed(int id);

Adafruit_NeoPixel pixels(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);

void getPixColor(byte ring, byte pix, bool active, byte mode)
{
  pixels.setPixelColor(led_mapping[pix] + ring * 16, colourLUT[2 * mode + active]);
}

void updatePixels()
{
  pixels.clear();
  if(channel_settings == -1)
  {
    uint8_t i, j;
    for (i = 0; i < 4; i++)
    {
      for (j = 0; j < ringStates[i].loopLength; j++)
      {
        if (bitRead(ringStates[i].euclidNotes, j))
        {
          if (ringStates[i].activeNote == j)
            getPixColor(i, j, true, 4);
          else
            getPixColor(i, j, true, ringStates[i].mode);
        }
        else
        {
          if (ringStates[i].activeNote == j)
            getPixColor(i, j, false, 4);
          else
            getPixColor(i, j, false, ringStates[i].mode);
        }
      }
    }
  }
  else
  {
    // div
    for (uint8_t i = 0; i < RINGPIX; i++)
    {
      if (!(i % divRatio[ringStates[channel_settings].clkDiv]))
        getPixColor(0, i, true, 3);
    }
    // pan
    switch (ringStates[channel_settings].panValue)
    {
      case 0: // hard left
        pixels.setPixelColor(led_mapping[7] + 16, 255, 0, 0);
        pixels.setPixelColor(led_mapping[8] + 16, 255, 0, 0);
        break;
      case 1 ... 8:
        pixels.setPixelColor(led_mapping[ringStates[channel_settings].panValue + 6] + 16, 128, 128, 128);
        break;

      case 9: // center
        pixels.setPixelColor(led_mapping[14] + 16, 0, 255, 0);
        pixels.setPixelColor(led_mapping[15] + 16, 0, 255, 0);
        break;

      case 10 ... 17:
        pixels.setPixelColor(led_mapping[(ringStates[channel_settings].panValue + 5) % 16] + 16, 128, 128, 128);
        break;

      case 18: // hard right
        pixels.setPixelColor(led_mapping[5] + 16, 0, 0, 255);
        pixels.setPixelColor(led_mapping[6] + 16, 0, 0, 255);
        break;
      
      default:
        break;
    }

    //vol
    for (uint8_t i = 1; i <= ringStates[channel_settings].vol; i++)
    {
      pixels.setPixelColor(led_mapping[i - 1] + 2 * 16, volLUT[i]);
    }
    //reverb
    for (uint8_t i = 1; i <= reverbLevel; i++)
    {
      pixels.setPixelColor(led_mapping[i - 1] + 3 * 16, (i * 8), 0, (i * 16) - 1);
    }
  }
  
/*
    case 3: // div
    {
      for (j = 0; j < ringStates[i].loopLength; j++)
      {
        if (!(j % divRatio[ringStates[i].clkDiv]))
          getPixColor(i, j, true, ringStates[i].mode);
      }
      break;
    }
*/
//    case 4: // pan
//      break;
  pixels.show();
}

const uint8_t max_div = 16;
uint8_t seq_counter = 1;


volatile bool midi_clock = false;
volatile uint8_t bpm = 120;
uint8_t pulse_counter = 0;


void sequencerTick()
{
  for (int i = 0; i < RINGS; i++)
  {
    if (seq_counter % divRatio[ringStates[i].clkDiv] == 0)
    {
      if (ringStates[i].activeNote + 1 >= ringStates[i].loopLength)
        ringStates[i].activeNote = 0;
      else
        ringStates[i].activeNote += 1;

      if (bitRead(ringStates[i].euclidNotes, ringStates[i].activeNote) || buttonState[i])
      {
        playerSampleOn(i);
      }
      updatePixels();
    }
  }
  if ((seq_counter + 1) > max_div)
    seq_counter = 1;
  else
    seq_counter += 1;
}

void startHandler()
{
  midi_clock = true;
  seq_counter = 1;
  for (int i = 0; i < RINGS; i++)
  {
    ringStates[i].activeNote = 0;
  }
  pulse_counter = 0;
  sequencerTick();
  Serial.println("clk start");
}

void stopHandler()
{
  midi_clock = false;
  pulse_counter = 0;
  Serial.println("clk stop");
}

void clockHandler()
{
  if (midi_clock)
  {
    pulse_counter++;
    if (! (pulse_counter % 3))
    {
      sequencerTick();
    }
    if (pulse_counter > 23) {
      pulse_counter = 0;
    }
  }
}

void rotaryEncoderChanged(bool clockwise, int id)
{
  if(channel_settings == -1)
  {
    bool recalcEuclid = true;
    switch (ringStates[id].mode)
    {
      case 0: // numNotes
        if (clockwise)
          ringStates[id].numNotes = constrain(ringStates[id].numNotes + 1, 0, ringStates[id].loopLength - 1);
        else
          ringStates[id].numNotes = constrain(ringStates[id].numNotes - 1, 0, ringStates[id].loopLength - 1);
        break;

      case 1: // offset edit
        if (clockwise)
          ringStates[id].offset = constrain(ringStates[id].offset + 1, 0, ringStates[id].loopLength - 1);
        else
          ringStates[id].offset = constrain(ringStates[id].offset - 1, 0, ringStates[id].loopLength - 1);
        break;

      case 2: // length edit
        if (clockwise)
          ringStates[id].loopLength = constrain(ringStates[id].loopLength + 1, 1, RINGPIX);
        else
          ringStates[id].loopLength = constrain(ringStates[id].loopLength - 1, 1, RINGPIX);
        break;
      default:
        break;
    }

    if (ringStates[id].numNotes > 0)
      ringStates[id].euclidNotes = euclid(ringStates[id].loopLength, ringStates[id].numNotes, ringStates[id].offset);
    else
      ringStates[id].euclidNotes = 0;
  }
  else
  {
    switch (id)
    {
    case 0: // div
      if (clockwise)
      {
        ringStates[channel_settings].clkDiv = constrain(ringStates[channel_settings].clkDiv + 1, 0, 4);
      }
      else
      {
        ringStates[channel_settings].clkDiv = constrain(ringStates[channel_settings].clkDiv - 1, 0, 4);
      }
      break;

    case 1: // pan
      if (clockwise)
      {
        ringStates[channel_settings].panValue = constrain(ringStates[channel_settings].panValue + 1, 0, 18);
        playerSetPan(channel_settings, ringStates[channel_settings].panValue);
      }
      else
      {
        ringStates[channel_settings].panValue = constrain(ringStates[channel_settings].panValue - 1, 0, 18);
        playerSetPan(channel_settings, ringStates[channel_settings].panValue);
      }
      break;

    case 2: // vol
      if (clockwise)
      {
        ringStates[channel_settings].vol = constrain(ringStates[channel_settings].vol + 1, 0, 16);
        playerSetVol(channel_settings, ringStates[channel_settings].vol);
      }
      else
      {
        ringStates[channel_settings].vol = constrain(ringStates[channel_settings].vol - 1, 0, 16);
        playerSetVol(channel_settings, ringStates[channel_settings].vol);
      }
      break;

    case 3: // reverb
      if (clockwise)
      {
        reverbLevel = constrain(reverbLevel + 1, 0, 16);
        Reverb_SetLevel(0, (float)reverbLevel / 16);
      }
      else
      {
        reverbLevel = constrain(reverbLevel - 1, 0, 16);
        Reverb_SetLevel(0, (float)reverbLevel / 16);
      }
      break;
    
    default:
      break;
    }
  }

#ifdef DEBUG
  Serial.println("Encoder " + String(id) + ": " + (clockwise ? String(" cw") : String("ccw")) 
              + ", Mode: " + String(ringStates[id].mode) 
              + ", loopLength: " + String(ringStates[id].loopLength) 
              + ", numNotes: " + String(ringStates[id].numNotes) 
              + ", div: " + String(ringStates[id].clkDiv)
              + ", pan: " + String(ringStates[id].panValue) 
              + ", offset: " + String(ringStates[id].offset));
#endif
  updatePixels();
}

void initRotaryEncoders()
{
  for (auto &rotaryEncoder : rotaryEncoders)
  {
    rotaryEncoder.init();
  }
}

void initButtons()
{
  for (auto &button : buttons)
  {
    button->begin();
  }
}

void pollMCP()
{
  // Read all MCPs and feed the input to each encoder and shortcut button.
  // This is more efficient than reading one pin state at a time

  uint16_t gpioAB = mcp.readGPIOAB();

  for (auto &rotaryEncoder : rotaryEncoders)
  {
    // Only feed this in the encoder if this is coming from the correct MCP
    if (rotaryEncoder.getMCP() == &mcp)
    {
      rotaryEncoder.feedInput(gpioAB);
    }
  }

  for (auto &button : buttons)
  {
    if (button->getMcp() == &mcp)
    {
      button->feedInput(gpioAB);
    }
  }
}

void buttonChanged(Button *btn, bool released, bool longPress)
{
#ifdef DEBUG
  Serial.print("button #");
  Serial.print(btn->id);
  Serial.print(" ");
  Serial.print((released) ? "RELEASED " : "PRESSED ");
  Serial.println((longPress) ? "LONG" : "SHORT");
#endif
  if (btn->id <= 3)
  {
    if (released)
    {
      if(longPress)
      {
        if(channel_settings < 0)
          channel_settings = btn->id;
        else
          channel_settings = -1;
      }
      else
      {
        if (ringStates[btn->id].mode < (numModes - 1))
          ringStates[btn->id].mode += 1;
        else
          ringStates[btn->id].mode = 0;
      }
    }
    updatePixels();
  }
  else if (btn->id <= 7)
  {
    if (!released)
    {
      buttonState[btn->id - 4] = true;
    }
    else
    {
      buttonState[btn->id - 4] = false;
    }
  }
}

void initAnimation()
{

  uint16_t i;
  for (i = 0; i < pixels.numPixels(); i++)
  {
    pixels.clear();
    for (size_t j = 0; j < 8; j++)
    {
      byte modifier = 255 >> j;
      pixels.setPixelColor(i - j, pixels.Color(modifier, 0, modifier));
    }
    pixels.show();
    delay(25);
  }
}
inline void Core0TaskSetup()
{
  newTime = micros(); //start the bpm timer clock
  initRotaryEncoders();
}

inline void Core0TaskLoop()
{
  updatePixels();
  pollMCP();
  if (! midi_clock)
  {
    INTERVAL = 15000000 / (bpm);
    if ((micros() - oldTime) > INTERVAL)
    {
      oldTime = micros();
      sequencerTick();
    }
  }
}

void CoreTask0(void *parameter)
{
  Core0TaskSetup();

  while (true)
  {
    Core0TaskLoop();
    /* this seems necessary to trigger the watchdog */
    delay(1);
    yield();
  }
}

void midiInit()
{
  // MIDI stuff
  Serial1.begin(31250, SERIAL_8N1, 34, 0);
  serialMidi.setHandleClock(clockHandler);
  serialMidi.setHandleStop(stopHandler);
  serialMidi.setHandleStart(startHandler);
  serialMidi.begin(MIDI_CHANNEL_OMNI);
}

void setup()
{
  Serial.begin(115200);
  midiInit();
  Serial.println("init...");
  pixels.begin();

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  I2C1.begin(I2C1_SDA, I2C1_SCL, (uint32_t)I2C1_SPEED); // Start I2C1 on pins 21 and 22
  // I2C2.begin(I2C2_SDA, I2C2_SCL, I2C2_SPEED); // Start I2C2 on pins 0 and 23

  // ES8388_Setup(I2C2);
  setup_i2s();
#if 0
    setup_wifi();
#else
  WiFi.mode(WIFI_OFF);
  btStop();
#endif
  playerInit();
  static float *revBuffer = (float *)malloc(sizeof(float) * REV_BUFF_SIZE);
  Reverb_Setup(revBuffer);
  Reverb_SetLevel(0, 0.2f);
  Delay_Init();
  Delay_Reset();

  PatchManager_SetDestination(1, 1);
  playerLoadWav(0, "/samples/0.wav");
  playerLoadWav(1, "/samples/1.wav");
  playerLoadWav(2, "/samples/2.wav");
  playerLoadWav(3, "/samples/3.wav");

  mcp.begin(I2C1);
  initRotaryEncoders();
  initButtons();
  initAnimation();
  Serial.println("done...");
  pixels.clear();

  for (size_t i = 0; i < RINGS; i++)
  {
    // Initialize structs
    ringStates[i].loopLength = 16;
    ringStates[i].clkDiv = 2;
    ringStates[i].panValue = 9;
    ringStates[i].vol = 16;
  }
  xTaskCreatePinnedToCore(CoreTask0, "CoreTask0", 8192, NULL, 999, &Core0TaskHnd, 0);
}

void loop()
{
  audio_task();
  serialMidi.read();
}
