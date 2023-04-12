/// @file    TwinkleFox.ino
/// @brief   Twinkling "holiday" lights that fade in and out.
/// @example TwinkleFox.ino

#include "FastLED.h"
const int ledPins[5] = {3, 5, 9, 11};
const int btnPins[3] = {A0, 10, A7};
const int ptiPins[2] = {A5, A6};
#define NUM_LEDS      300
#define LED_TYPE   WS2811
#define COLOR_ORDER   GRB
#define DATA_PIN        2
//#define CLK_PIN       4
#define VOLTS          12
#define MAX_MA       4000

CRGBArray<NUM_LEDS> leds;

// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).  
// 4, 5, and 6 are recommended, default is 4.
int TWINKLE_SPEED = 4;

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).  
// Default is 5.
int TWINKLE_DENSITY = 2;

// Background color for 'unlit' pixels
// Can be set to CRGB::Black if desired.
CRGB gBackgroundColor = CRGB::FairyLight; 
// Example of dim incandescent fairy light background color
// CRGB gBackgroundColor = CRGB(CRGB::FairyLight).nscale8_video(16);

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries 
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0

// If COOL_LIKE_INCANDESCENT is set to 1, colors will 
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 0

CRGBPalette16 gCurrentPalette;
CRGBPalette16 gTargetPalette;

class BeatCounter {
  //counts midi beat-messages and calculates current number of beats
  private:
    int16_t beats; //number of fully passed beats
    const int16_t COUNTSPERBEAT = 24; //number of timing messages within a quater beat
    void (*onNewBeat)(int16_t); // Callback function to notify the caller about the end of a beat
    int16_t lengths[5] = {8, 16, 32, 64, 128};
    int currLength = 0;

  public:
    int16_t length; //length of the bar in beats
    int16_t counts; //counted timing messages since last beat

    BeatCounter& operator=(BeatCounter&& other) {
      counts = other.counts;
      beats = other.beats;
      length = other.length;
      onNewBeat = other.onNewBeat;
      return *this;
    }

    BeatCounter(){
      //empty default constructor
    }

    BeatCounter(int16_t barLength, void (*newBeatCallback)(int16_t)) {
      //length in number of single beats
      length = barLength;
      onNewBeat = newBeatCallback;
      Reset();
    }

    int16_t GetValue() {
      //returns current value in 0..100
      return ((beats * COUNTSPERBEAT + counts) * 100) / (length * COUNTSPERBEAT);
    }

    int16_t GetBeats(){
      //returns the counter value as beats
      return beats;
    }

    void Increment(){
      counts++;
      if(counts >= COUNTSPERBEAT){
        //12 timing-counts = one beat, actually one quater-beat, but here referred to as a beat
        counts = 0;
        beats++;
        if(beats >= length){
          //length reached, reset counter
          Reset();
        } else{
          onNewBeat(beats);
        }       
      }
    }

    void Reset(){
      counts = 0;
      beats = 0;
      onNewBeat(0);
    }

    void changeLength(bool up){
      if (up) {
        currLength = (currLength + 1) % 5;
      } else {
        currLength = (currLength - 1 + 5) % 5;
      }
      length = lengths[currLength];
      beats = ((beats) + length) % length;
      Serial.print((String)length + "\n");
    }

};

// Define enum for message types
enum MidiMessageTypes {
  Invalid = 0x00,                 // zero as invalid message
  NoteOff = 0x80,                 // Data byte 1: note number, Data byte 2: velocity
  NoteOn = 0x90,                  // Data byte 1: note number, Data byte 2: velocity
  PolyphonicAftertouch = 0xA0,    // Data byte 1: note number, Data byte 2: pressure
  ControlChange = 0xB0,           // Data byte 1: controller number, Data byte 2: controller value
  ProgramChange = 0xC0,           // Data byte 1: program number
  ChannelAftertouch = 0xD0,       // Data byte 1: pressure
  PitchBend = 0xE0,               // Data byte 1: LSB, Data byte 2: MSB (14-bit value)
  SystemExclusive = 0xF0,         // Data bytes: variable length (sysex message)
  TimeCodeQuarterFrame = 0xF1,    // Data byte 1: time code value
  SongPositionPointer = 0xF2,     // Data byte 1: LSB, Data byte 2: MSB (14-bit value)
  SongSelect = 0xF3,              // Data byte 1: song number
  TuneRequest = 0xF6,             // No data bytes
  EndOfExclusive = 0xF7,          // No data bytes (end of sysex message)
  TimingClock = 0xF8,             // No data bytes
  Start = 0xFA,                   // No data bytes
  Continue = 0xFB,                // No data bytes
  Stop = 0xFC,                    // No data bytes
  ActiveSensing = 0xFE,           // No data bytes
  Reset = 0xFF                    // No data bytes
};

class MidiReader {
  //This class reads midi input on Rx pin. It detects the first byte of each message by checking it's Most Significant Bit (MSB)
  //it then reads 0-2 more bytes depending on the message type that's used as data.
  //Example for a message with 2 data bytes:
  //Byte1: 0x90 (Note on) - Byte2: 0x3C (Note pitch: C4) - Byte3: 0x45 (medium velocity)
  //Example for a message with no data bytes:
  //Byte1: 0xF1 (Timing message)
  private:
    // Callback function to notify the caller about received midi messages
    void (*onNewMidi)(MidiMessageTypes, int, int, int);
    void (*onConnect)(bool); //called on connect or disconnect with status as parameter
    // Buffer for incoming midi messages
    int midiBuffer[3];
    uint8_t bufferIndex = 0;
    int connectionTimeout = 500; //time after which connected gets set to false
    int connectionCounter = 0;
    int lastMlls = millis();
    unsigned long lastTimingClockTime = 0;
    int timingClockCounter = 0;

  public:
    bool running = false; //is false after midi stop message, and back true after start/continue
    bool connected = false; //gets set to false after connectionTimeout
    float bpm = 0.0f;

    MidiReader(){}

    MidiReader(void (*newMidiCallback)(MidiMessageTypes, int, int, int), void(*onConnectCallback)(bool)) {
      onNewMidi = newMidiCallback;
      onConnect = onConnectCallback;
    }

    // Method to be called in the main loop
    void update() { 
      int mlls = millis();
      int deltat = mlls-lastMlls;
      lastMlls = mlls;
      connectionCounter += deltat;
      if(connectionCounter >= connectionTimeout){
        connected = false;
        onConnect(false);
      }
      while (Serial.available() > 0) {
        if(!connected){
          onConnect(true);
        }
        connected = true; //midi recieved == connected
        connectionCounter = 0;
        uint8_t incomingByte = Serial.read();
        // Serial.print(incomingByte, HEX);
        // Serial.print("\n");
        if(incomingByte & 0x80){
          //is command byte, MSB == 1
          midiBuffer[0] = incomingByte;
          bufferIndex = 1;
        } else {
          // databyte, MSB == 0
          if(bufferIndex > 0){
            midiBuffer[bufferIndex] = incomingByte;
            bufferIndex++;
          } else {
            //error, databyte recieved without command byte
          }
        }
        int val = midiBuffer[0];
        if(val & 0x80){
          MidiMessageTypes type = 0;
          uint8_t channel = 0;
          //check if message complete
          if(midiBuffer[0] >= 0xF0){
            //is System Message 0xF0..0xFF
            type = (MidiMessageTypes)midiBuffer[0];
          } else{
            //is channel message
            type = (MidiMessageTypes)(midiBuffer[0] & 0xF0);
            channel = midiBuffer[0] & 0x0F;
          }
          switch(type){
            case Invalid:
              //Invalid Message
            break;

            case NoteOn:
            case NoteOff:
            case PolyphonicAftertouch:
            case ControlChange:
            case PitchBend:
            case SongPositionPointer:
            case SystemExclusive:
              //messages that require 2 data bytes
              if(bufferIndex == 2){
                //enough data for NoteOn Message available
                onNewMidi(type, channel, midiBuffer[1], midiBuffer[2]);   
                bufferIndex = 0;           
              }
            break;

            case ProgramChange:
            case ChannelAftertouch:
            case SongSelect:
            case TimeCodeQuarterFrame:
              //messages that require 1 data byte
              if(bufferIndex == 1){
                //enough data for NoteOn Message available
                onNewMidi(type, channel, midiBuffer[1], 0);  
                bufferIndex = 0;            
              }
            break;

            case Reset:
            case ActiveSensing:
            case Stop:
            case Start:
            case Continue:
            case TimingClock:
            case EndOfExclusive:
            case TuneRequest:
              //messages that require no data bytes
              if(type == Start || type == Continue){
                running = true;
              } else if (type == Stop){
                running = false;
              } else if (type == TimingClock){
                // Calculate BPM
                timingClockCounter++;
                if (timingClockCounter == 24) {
                  unsigned long currentMillis = millis();
                  unsigned long deltaTime = currentMillis - lastTimingClockTime;
                  lastTimingClockTime = currentMillis;
                  if (deltaTime > 0) {
                    bpm = (60.0f * 1000) / deltaTime; // Multiply by 1000 to account for milliseconds
                  }
                  timingClockCounter = 0;
                }
              }
              onNewMidi(type, 0, 0, 0);
              bufferIndex = 0;
            break;

            default:
              // //Unknown message recieved
              // Serial.print("unknown Message: ");
              // Serial.print(type, HEX);
              // Serial.print("\n");
            break;
          }          
        }
      }
    }
};

class Led{
  private:
    int pin;
    int brightness = 2;
    int brightCounter = 0;
    int brightPeriod = 10;
    int blinkRate = 0;
    int blinkDuty = 0;
    int blinkCount = 0;
    int blinkCounter = 0;
    int lastMlls;
    bool state;
    bool inverted = false; //Set to true, depending on how you connected the LED
    bool timerPwm = false; //if a pwm-pin is used, use the pwm feature to get better resolution

    int invertBrightness(int brightness){
      return 255 - brightness;
    }

  public:
    Led(){
      
    }

    Led(int pin){
      //
      lastMlls = millis();
      this->pin = pin;
      pinMode(pin, OUTPUT);
      brightCounter = brightPeriod;
      switch(pin){
        case 3:
        case 5:
        case 6:
        case 9:
        case 10:
        case 11:
          //is PWM-Pin - use timer-pwm
          timerPwm = true;
        break;
        default:
          //not pwm-pin, use software timed pwm
          timerPwm = false;
        break;
      }
      setPin(false);
    }

    void update(){
      int mls = millis();
      int deltat = mls - lastMlls;
      lastMlls = mls;
      bool dimmed = false;
      //Dimming of the LED
      if(brightCounter > 0){
        brightCounter -= deltat;
        if(brightCounter < invertBrightness(brightness)/26){
          dimmed = false;
        } else {
          dimmed = true;
        }
        if(brightCounter <= 0){
          brightCounter = brightPeriod;
        }
      }
      //Blinking of the LED
      if(blinkCounter > 0){
        //blinking
        blinkCounter -= deltat;
        setPin(blinkCounter > (blinkRate - blinkDuty));
        if(blinkCounter <= 0){
          if(blinkCount != 0){
            if(blinkCount > 0){
              blinkCount--;
            }
            blinkCounter = blinkRate;
          }          
        }
      } else{
        //not blinking
        if(blinkCount != 0){
          if(blinkCount > 0){
            blinkCount --;
          }
          blinkCounter = blinkRate;
        }
        setPin(state && (timerPwm || dimmed));
      }
    }

    void setPin(bool state){
      int onBright = timerPwm?brightness:255; //set brightness to 255 if not PWM-Pin, state will be toggled from update() to dim
      int offBright = 0;
      if(inverted){
        onBright = invertBrightness(onBright);
        offBright = 255;
      }
      analogWrite(pin, state?onBright:offBright);        
    }

    void setState(bool state){
      this->state = state;
    }

    void blink(int rate, int duty, int count){
      //sets to blink N times, count = -1 for infinite
      //rate in ms period time, 1000 slow rate, 10 fast rate
      //duty in 0..100 (%)
      blinkRate = rate;
      blinkDuty = (duty * rate)/100;
      blinkCount = count - 1; //decrement, since first blink started
      blinkCounter = rate;
    }

    void setBrightness(int brightness){
      //brightness in 0..10
      this->brightness = brightness;
    }
};

class LedBar{
  private:

  public:
    int length = 180;
    int pin;
    uint16_t position = 0; //position of the pointer
    bool highlighted = false;
    uint32_t markerColor = CRGB::Green;
    uint32_t highlightColor = CRGB::Blue;
    uint32_t pointerColor = CRGB::Red;
    int16_t MARKERS[8] = {0, 22, 45, 67, 90, 112, 135, 157};

    LedBar(){
    }

    void update(int beats, int beatsLength){
      highlighted = ((beats%4)+4)%4 == 0;
      position = (length * beats) / beatsLength;
    }

    bool isMarker(int pixel){
      bool is = false;
      for(int i=0; i<8; i++){
        is = is || MARKERS[i] == pixel;
      }
      return is;
    }
};

class Button{
  public:
    bool state = false; //current stable, debounced state, pressed down or not
    bool lastState = false; //last stable, debounced state
    bool pressed = false; //pressed down in this iteration?
    bool released = false; //released in this iteration?
    int pin = 0;
    bool analogPin = false;
    bool lastButtonState = false; //last state read from the pin, not debounced
    int lastDebounceTime = 0;
    int debounceDelay = 100;

    Button(){
      
    }

    Button(int pinNumber){
      //Initialize button input
      pin = pinNumber;
      pinMode(pinNumber, INPUT);
      switch(pinNumber){
        case A0:
        case A1:
        case A2:
        case A3:
        case A4:
        case A5:
        case A6:
        case A7:
          analogPin = true;
        break;
      }
    }

    void update(){
      //read current value
      lastState = state;
      bool reading = false;
      if(analogPin){
        reading = analogRead(pin) > 1000;
      } else{
        reading = digitalRead(pin);
      }

      //Debounce Button
      if (reading != lastButtonState) {
        lastDebounceTime = millis(); // Update the debounce timer
      }

      if ((millis() - lastDebounceTime) > debounceDelay) {
        // If the button state hasn't changed for debounceDelay milliseconds, it's considered a stable state
        if (reading != state) {
          state = reading;
        }
      }
      lastButtonState = reading;
      //Detect valuechange
      pressed = !lastState && state; //was pressed in this iteration
      released = lastState && !state; //was released in this iteration
    }
};

class Poti{
  private:
    int lastValue = 0;
  public:
    int pin = A0;
    int moveTolerance = 1; //turn up to reduce cpu load, keep on 1 to read poti as much as possible
    bool moved = false; //indicates if poti was moved this iteration by more than moveTolerance
    uint16_t value = 0;
    bool inverted = false;

    Poti(){}

    Poti(int pin){
      this->pin = pin;
      pinMode(pin, INPUT);
    }

    void update(){
      value = analogRead(pin);
      if(inverted){
        value = 1023 - value;
      }
      moved = abs(value-lastValue) > moveTolerance; //detect movement greater than moveTolerance
    }

};

#define LED_COUNT 4
Button leftBtn;
Button middBtn;
Button rghtBtn;
Poti leftPti;
Poti rghtPti;
Led boardLeds[LED_COUNT];
MidiReader midi;
BeatCounter counter;

void onConnect(bool connected){

}

void onMidi(MidiMessageTypes type, int channel, int byte1, int byte2){
  switch(type){
    case Start:
    case Continue:
      counter.Reset();
    break;
    case Stop:
      counter.Reset();
    break;
    case TimingClock:
      counter.Increment();
      // pacifica_loop();
      // FastLED.show();
      updateTwinkles();
    break;
  }
  // Serial.print((String)midi.bpm + "\n");
  // Serial.print("Midi Message: 0x");
  // Serial.print(type, HEX);
  // Serial.print(" / 0x");
  // Serial.print(channel, HEX);
  // Serial.print(" / 0x");
  // Serial.print(byte1, HEX);
  // Serial.print(" / 0x");
  // Serial.print(byte2, HEX);
  // Serial.print("\n");
}

void onBeat(int16_t beats){
  // TWINKLE_DENSITY = (beats != 0) * 3;
  boardLeds[1].blink(200, 50, 1);
  boardLeds[2].blink(200, 50, 1);
  if(beats == 0){
    boardLeds[0].blink(200, 50, 1);
    boardLeds[3].blink(200, 50, 1);
  }
}

//SETUP
void setup() {
  initTwinkles();
  Serial.begin(31250);
  delay( 1000 ); //safety startup delay
  //Init Buttons and Potis
  leftBtn = Button(btnPins[0]);
  middBtn = Button(btnPins[1]);
  rghtBtn = Button(btnPins[2]);
  leftPti = Poti(ptiPins[0]);
  leftPti.inverted = true;
  rghtPti = Poti(ptiPins[1]);
  //Init LEDs
  for(int i=0; i<LED_COUNT; i++){
    boardLeds[i] = Led(ledPins[i]);
    boardLeds[i].setState(false);
    boardLeds[i].blink(200, 30, 5);
  }
  midi = MidiReader(onMidi, onConnect);
  counter = BeatCounter(4, onBeat);
}

//LOOP
void loop()
{
  //Update buttons and potis
  leftBtn.update();
  middBtn.update();
  rghtBtn.update();
  leftPti.update();
  rghtPti.update();
  //update LEDs
  for(int i=0; i<LED_COUNT; i++){
    boardLeds[i].update();
  }
  if(leftBtn.pressed){
    TWINKLE_DENSITY = ((TWINKLE_DENSITY + 1 + 9) % 9);
  }
  if(middBtn.pressed){
    //change color palette
    chooseNextColorPalette( gTargetPalette ); 
  }
  if(rghtBtn.pressed){
    TWINKLE_SPEED = ((TWINKLE_SPEED + 1 + 9) % 9);
  }
  gBackgroundColor = CRGB(CRGB::FairyLight).nscale8_video(rghtPti.value/64 + 1);
  FastLED.setBrightness(leftPti.value/4);
  midi.update();
  if(!midi.connected){
    updateTwinkles();
  }
}

//TWINKLES CODE FROM HERE ON

//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette; a few palettes are provided.
//
//  This December 2015 implementation improves on the December 2014 version
//  in several ways:
//  - smoother fading, compatible with any colors and any palettes
//  - easier control of twinkle speed and twinkle density
//  - supports an optional 'background color'
//  - takes even less RAM: zero RAM overhead per pixel
//  - illustrates a couple of interesting techniques (uh oh...)
//
//  The idea behind this (new) implementation is that there's one
//  basic, repeating pattern that each pixel follows like a waveform:
//  The brightness rises from 0..255 and then falls back down to 0.
//  The brightness at any given point in time can be determined as
//  as a function of time, for example:
//    brightness = sine( time ); // a sine wave of brightness over time
//
//  So the way this implementation works is that every pixel follows
//  the exact same wave function over time.  In this particular case,
//  I chose a sawtooth triangle wave (triwave8) rather than a sine wave,
//  but the idea is the same: brightness = triwave8( time ).  
//  
//  Of course, if all the pixels used the exact same wave form, and 
//  if they all used the exact same 'clock' for their 'time base', all
//  the pixels would brighten and dim at once -- which does not look
//  like twinkling at all.
//
//  So to achieve random-looking twinkling, each pixel is given a 
//  slightly different 'clock' signal.  Some of the clocks run faster, 
//  some run slower, and each 'clock' also has a random offset from zero.
//  The net result is that the 'clocks' for all the pixels are always out 
//  of sync from each other, producing a nice random distribution
//  of twinkles.
//
//  The 'clock speed adjustment' and 'time offset' for each pixel
//  are generated randomly.  One (normal) approach to implementing that
//  would be to randomly generate the clock parameters for each pixel 
//  at startup, and store them in some arrays.  However, that consumes
//  a great deal of precious RAM, and it turns out to be totally
//  unnessary!  If the random number generate is 'seeded' with the
//  same starting value every time, it will generate the same sequence
//  of values every time.  So the clock adjustment parameters for each
//  pixel are 'stored' in a pseudo-random number generator!  The PRNG 
//  is reset, and then the first numbers out of it are the clock 
//  adjustment parameters for the first pixel, the second numbers out
//  of it are the parameters for the second pixel, and so on.
//  In this way, we can 'store' a stable sequence of thousands of
//  random clock adjustment parameters in literally two bytes of RAM.
//
//  There's a little bit of fixed-point math involved in applying the
//  clock speed adjustments, which are expressed in eighths.  Each pixel's
//  clock speed ranges from 8/8ths of the system clock (i.e. 1x) to
//  23/8ths of the system clock (i.e. nearly 3x).
//
//  On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels
//  smoothly at over 50 updates per seond.
//
//  -Mark Kriegsman, December 2015

void initTwinkles(){
  FastLED.setMaxPowerInVoltsAndMilliamps( VOLTS, MAX_MA);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip);

  chooseNextColorPalette(gTargetPalette);
}

void updateTwinkles(){  
  EVERY_N_MILLISECONDS( 10 ) {
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 12);
  }
  drawTwinkles( leds);
  FastLED.show();
}

//  This function loops over each pixel, calculates the 
//  adjusted 'clock' that this pixel should use, and calls 
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color, 
//  whichever is brighter.
void drawTwinkles( CRGBSet& L)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;
  
  uint32_t clock32 = millis();

  // Set up the background color, "bg".
  // if AUTO_SELECT_BACKGROUND_COLOR == 1, and the first two colors of
  // the current palette are identical, then a deeply faded version of
  // that color is used for the background color
  CRGB bg;
  if( (AUTO_SELECT_BACKGROUND_COLOR == 1) &&
      (gCurrentPalette[0] == gCurrentPalette[1] )) {
    bg = gCurrentPalette[0];
    uint8_t bglight = bg.getAverageLight();
    if( bglight > 64) {
      bg.nscale8_video( 16); // very bright, so scale to 1/16th
    } else if( bglight > 16) {
      bg.nscale8_video( 64); // not that bright, so scale to 1/4th
    } else {
      bg.nscale8_video( 86); // dim, scale to 1/3rd.
    }
  } else {
    bg = gBackgroundColor; // just use the explicitly defined background color
  }

  uint8_t backgroundBrightness = bg.getAverageLight();
  
  for( CRGB& pixel: L) {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    uint16_t myclockoffset16= PRNG16; // use that number as clock offset
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
    uint8_t myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
    uint32_t myclock30 = (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
    uint8_t  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

    // We now have the adjusted 'clock' for this pixel, now we call
    // the function that computes what color the pixel should be based
    // on the "brightness = f( time )" idea.
    CRGB c = computeOneTwinkle( myclock30, myunique8);

    uint8_t cbright = c.getAverageLight();
    int16_t deltabright = cbright - backgroundBrightness;
    if( deltabright >= 32 || (!bg)) {
      // If the new pixel is significantly brighter than the background color, 
      // use the new color.
      pixel = c;
    } else if( deltabright > 0 ) {
      // If the new pixel is just slightly brighter than the background color,
      // mix a blend of the new color and the background color
      pixel = blend( bg, c, deltabright * 8);
    } else { 
      // if the new pixel is not at all brighter than the background color,
      // just use the background color.
      pixel = bg;
    }
  }
}

//  This function takes a time in pseudo-milliseconds,
//  figures out brightness = f( time ), and also hue = f( time )
//  The 'low digits' of the millisecond time are used as 
//  input to the brightness wave function.  
//  The 'high digits' are used to select a color, so that the color
//  does not change over the course of the fade-in, fade-out
//  of one cycle of the brightness wave function.
//  The 'high digits' are also used to determine whether this pixel
//  should light at all during this cycle, based on the TWINKLE_DENSITY.
CRGB computeOneTwinkle( uint32_t ms, uint8_t salt)
{
  uint16_t ticks = ms >> (8-TWINKLE_SPEED);
  uint8_t fastcycle8 = ticks;
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8( slowcycle16);
  slowcycle16 =  (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);
  
  uint8_t bright = 0;
  if( ((slowcycle8 & 0x0E)/2) < TWINKLE_DENSITY) {
    bright = attackDecayWave8( fastcycle8);
  }

  uint8_t hue = slowcycle8 - salt;
  CRGB c;
  if( bright > 0) {
    c = ColorFromPalette( gCurrentPalette, hue, bright, NOBLEND);
    if( COOL_LIKE_INCANDESCENT == 1 ) {
      coolLikeIncandescent( c, fastcycle8);
    }
  } else {
    c = CRGB::Black;
  }
  return c;
}

// This function is like 'triwave8', which produces a 
// symmetrical up-and-down triangle sawtooth waveform, except that this
// function produces a triangle wave with a faster attack and a slower decay:
//
//     / \ 
//    /     \ 
//   /         \ 
//  /             \ 
//

uint8_t attackDecayWave8( uint8_t i)
{
  if( i < 86) {
    return i * 3;
  } else {
    i -= 86;
    return 255 - (i + (i/2));
  }
}

// This function takes a pixel, and if its in the 'fading down'
// part of the cycle, it adjusts the color a little bit like the 
// way that incandescent bulbs fade toward 'red' as they dim.
void coolLikeIncandescent( CRGB& c, uint8_t phase)
{
  if( phase < 128) return;

  uint8_t cooling = (phase - 128) >> 4;
  c.g = qsub8( c.g, cooling);
  c.b = qsub8( c.b, cooling * 2);
}

// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM =
{  CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
   CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
   CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray, 
   CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green };

// A mostly (dark) green palette with red berries.
#define Holly_Green 0x00580c
#define Holly_Red   0xB00402
const TProgmemRGBPalette16 Holly_p FL_PROGMEM =
{  Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
   Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
   Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
   Holly_Green, Holly_Green, Holly_Green, Holly_Red 
};

// A red and white striped palette
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM =
{  CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, 
   CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray,
   CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, 
   CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray };

// A mostly blue palette with white accents.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM =
{  CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
   CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
   CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
   CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray };

// A pure "fairy light" palette with some brightness variations
#define HALFFAIRY ((CRGB::FairyLight & 0xFEFEFE) / 2)
#define QUARTERFAIRY ((CRGB::FairyLight & 0xFCFCFC) / 4)
const TProgmemRGBPalette16 FairyLight_p FL_PROGMEM =
{  CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, 
   HALFFAIRY,        HALFFAIRY,        CRGB::FairyLight, CRGB::FairyLight, 
   QUARTERFAIRY,     QUARTERFAIRY,     CRGB::FairyLight, CRGB::FairyLight, 
   CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight };

// A palette of soft snowflakes with the occasional bright one
const TProgmemRGBPalette16 Snow_p FL_PROGMEM =
{  0x304048, 0x304048, 0x304048, 0x304048,
   0x304048, 0x304048, 0x304048, 0x304048,
   0x304048, 0x304048, 0x304048, 0x304048,
   0x304048, 0x304048, 0x304048, 0xE0F0FF };

// A palette reminiscent of large 'old-school' C9-size tree lights
// in the five classic colors: red, orange, green, blue, and white.
#define C9_Red    0xB80400
#define C9_Orange 0x902C02
#define C9_Green  0x046002
#define C9_Blue   0x070758
#define C9_White  0x606820
const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM =
{  C9_Red,    C9_Orange, C9_Red,    C9_Orange,
   C9_Orange, C9_Red,    C9_Orange, C9_Red,
   C9_Green,  C9_Green,  C9_Green,  C9_Green,
   C9_Blue,   C9_Blue,   C9_Blue,
   C9_White
};

// A cold, icy pale blue palette
#define Ice_Blue1 0x0C1040
#define Ice_Blue2 0x182080
#define Ice_Blue3 0x5080C0
const TProgmemRGBPalette16 Ice_p FL_PROGMEM =
{
  Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
  Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
  Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
  Ice_Blue2, Ice_Blue2, Ice_Blue2, Ice_Blue3
};

// Add or remove palette names from this list to control which color
// palettes are used, and in what order.
const TProgmemRGBPalette16* ActivePaletteList[] = {
  &BlueWhite_p,
  &RetroC9_p,
  &RainbowColors_p,
  &FairyLight_p,
  &RedGreenWhite_p,
  &PartyColors_p,
  &RedWhite_p,
  &Snow_p,
  &Holly_p,
  &Ice_p  
};


// Advance to the next color palette in the list (above).
void chooseNextColorPalette( CRGBPalette16& pal)
{
  const uint8_t numberOfPalettes = sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);
  static uint8_t whichPalette = -1; 
  whichPalette = addmod8( whichPalette, 1, numberOfPalettes);

  pal = *(ActivePaletteList[whichPalette]);
}

//Pacific Example from FastLED


//////////////////////////////////////////////////////////////////////////
//
// The code for this animation is more complicated than other examples, and 
// while it is "ready to run", and documented in general, it is probably not 
// the best starting point for learning.  Nevertheless, it does illustrate some
// useful techniques.
//
//////////////////////////////////////////////////////////////////////////
//
// In this animation, there are four "layers" of waves of light.  
//
// Each layer moves independently, and each is scaled separately.
//
// All four wave layers are added together on top of each other, and then 
// another filter is applied that adds "whitecaps" of brightness where the 
// waves line up with each other more.  Finally, another pass is taken
// over the led array to 'deepen' (dim) the blues and greens.
//
// The speed and scale and motion each layer varies slowly within independent 
// hand-chosen ranges, which is why the code has a lot of low-speed 'beatsin8' functions
// with a lot of oddly specific numeric ranges.
//
// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https://goo.gl/maps/QQgd97jjHesHZVxQ7
//
CRGBPalette16 pacifica_palette_1 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
CRGBPalette16 pacifica_palette_2 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
CRGBPalette16 pacifica_palette_3 = 
    { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33, 
      0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };


void pacifica_loop()
{
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011,10,13));
  sCIStart2 -= (deltams21 * beatsin88(777,8,11));
  sCIStart3 -= (deltams1 * beatsin88(501,5,7));
  sCIStart4 -= (deltams2 * beatsin88(257,4,6));

  // Clear out the LED array to a dim background blue-green
  fill_solid( leds, NUM_LEDS, CRGB( 2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer( pacifica_palette_1, sCIStart1, beatsin16( 3, 11 * 256, 14 * 256), beatsin8( 10, 70, 130), 0-beat16( 301) );
  pacifica_one_layer( pacifica_palette_2, sCIStart2, beatsin16( 4,  6 * 256,  9 * 256), beatsin8( 17, 40,  80), beat16( 401) );
  pacifica_one_layer( pacifica_palette_3, sCIStart3, 6 * 256, beatsin8( 9, 10,38), 0-beat16(503));
  pacifica_one_layer( pacifica_palette_3, sCIStart4, 5 * 256, beatsin8( 8, 10,28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
}

// Add one layer of waves into the led array
void pacifica_one_layer( CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16( waveangle ) + 32768;
    uint16_t cs = scale16( s16 , wavescale_half ) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16( ci) + 32768;
    uint8_t sindex8 = scale16( sindex16, 240);
    CRGB c = ColorFromPalette( p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps()
{
  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );
  
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if( l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8( overage, overage);
      leds[i] += CRGB( overage, overage2, qadd8( overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors()
{
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].blue = scale8( leds[i].blue,  145); 
    leds[i].green= scale8( leds[i].green, 200); 
    leds[i] |= CRGB( 2, 5, 7);
  }
}