#include <Rotary.h>
#include <RotaryEncOverMCP.h>
#include "Button.h"
#include "Multiplexer.h"

// Pins for MCP23017
#define GPA0 0
#define GPA1 1
#define GPA2 2
#define GPA3 3
#define GPA4 4
#define GPA5 5
#define GPA6 6
#define GPA7 7
#define GPB0 8
#define GPB1 9
#define GPB2 10
#define GPB3 11
#define GPB4 12
#define GPB5 13
#define GPB6 14
#define GPB7 15

Adafruit_MCP23017 mcp;

/* Function prototypes */
void buttonChanged(Button *btn, bool released, bool longPress);
void rotaryEncoderChanged(bool clockwise, int id);


/* Array of all rotary encoders and their pins */
RotaryEncOverMCP rotaryEncoders[] = {
        // outputA,B on GPA7,GPA6, register with callback and ID=1
        RotaryEncOverMCP(&mcp, GPA1, GPA2, &rotaryEncoderChanged, 0),
        RotaryEncOverMCP(&mcp, GPB1, GPB0, &rotaryEncoderChanged, 1),
        RotaryEncOverMCP(&mcp, GPB4, GPB3, &rotaryEncoderChanged, 2),
        RotaryEncOverMCP(&mcp, GPB7, GPB6, &rotaryEncoderChanged, 3),        
};

Button rotaryButton1 = Button(&mcp, GPB5, 3, &buttonChanged);
Button rotaryButton2 = Button(&mcp, GPB2, 2, &buttonChanged);
Button rotaryButton3 = Button(&mcp, GPA0, 1, &buttonChanged);
Button rotaryButton4 = Button(&mcp, GPA3, 0, &buttonChanged);

Button button1 = Button(&mcp, GPA4, 7, &buttonChanged);
Button button2 = Button(&mcp, GPA5, 6, &buttonChanged);
Button button3 = Button(&mcp, GPA6, 5, &buttonChanged);
Button button4 = Button(&mcp, GPA7, 4, &buttonChanged);

Button *buttons[] = {
    &rotaryButton1, &rotaryButton2, &rotaryButton3, &rotaryButton4, &button1, &button2, &button3, &button4
};
