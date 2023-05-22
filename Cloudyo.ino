/// @file    TwinkleFox.ino
/// @brief   Twinkling "holiday" lights that fade in and out.
/// @example TwinkleFox.ino
#include <Arduino.h>
#include <Wire.h>
#include "FastLED.h"
#define NUM_LEDS      300
#define LED_TYPE   WS2811
#define DATA_PIN        10

class InputDevice {
  //reads data through I2C. Since Serial communication is not possible while using FastLED, this is used to recieve data
  //from a different arduino that gets the data through serial from bluetooth module, dmx module or maybe something else
protected:
  int* channels;
  int channelCount;
  uint8_t buffer[5];  // Buffer to store incoming bytes
  int bufferIndex = 0;  // Current position in the buffer

public:
  InputDevice(int _channelCount) {
    channelCount = _channelCount;
    channels = new int[channelCount];
    for(int i = 0; i < channelCount; i++) {
      channels[i] = 0;
    }
  }

  virtual ~InputDevice() {
    delete[] channels;
  }

  int getValue(int channel) {
    return channels[channel-1]; //channel is 1-based
  }

  int getValueScaled(int channel, int min, int max) {
    return map(channels[channel-1], 0, 255, min, max); //channel is 1-based
  }
  // function that executes whenever data is received from master
  void receiveEvent(int howMany) {
    while (Wire.available()) { // loop through all
      uint8_t incoming = Wire.read();
      
      // Check if a new message should start
      if (incoming > 127) {
        bufferIndex = 0;
        buffer[0] = 255;
      }

      // Save incoming byte into buffer
      buffer[bufferIndex++] = incoming;

      // Check if a complete message was received
      if (bufferIndex >= 5) {
        int channelNum = buffer[1] + buffer[2];
        channelNum -= 1; //channels that come in are 1-based
        int value = buffer[3] + buffer[4];
        if (channelNum < channelCount) {
          channels[channelNum] = value;
        }
        bufferIndex = 0;  // Reset the buffer index for the next message
      }
    }
    Serial.print(channels[0]);
    Serial.println();
  }

};

CRGBArray<NUM_LEDS> leds;

// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).  
// 4, 5, and 6 are recommended, default is 4.
int TWINKLE_SPEED = 2;

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).  
// Default is 5.
int TWINKLE_DENSITY = 1;

// Background color for 'unlit' pixels
// Can be set to CRGB::Black if desired.
CRGB gBackgroundColor = CRGB::Black; 
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

class Strobo{
  private:
    void toggle(){
      state = !state;
      fill_solid(FastLED.leds(), NUM_LEDS, (state?color:CRGB::Black).nscale8_video(brightness));
    }
  public:
    int rate = 500;
    int lastMlls = millis();
    int state = false;
    int speed = 0; //set this to >0 to ignore the rate-property and use speed which counts calls of update(). Use this to keep animation synced to midi
    int speedCounter = 0;
    int brightness = 255;
    CRGB color = CRGB::White;

    Strobo(){}

    void update(){
      if(speed > 0){
        speedCounter++;
        if(speedCounter >= speed){
          toggle();
          speedCounter = 0;
        }
      } else {
        int mlls = millis();
        if(mlls - lastMlls >= rate){
          toggle();
          lastMlls = mlls;
        }        
      }
    }
    
    void setColor(int value){
      //set color with value 0..1023 going through a hue rainbow
      color = mapValueToRainbowColor(value);
    }

    CRGB mapValueToRainbowColor(int value) {
      // Map the input value to the hue range (0-255) in the CHSV color space
      uint8_t hue = map(value, 0, 1023, 0, 255);
      CHSV hsvColor = CHSV(hue, 255, 255); // Full saturation and value for a bright rainbow color
      return hsvColor;
    }
};

class Worm {
  public:
    int speed = 1;
    int position = 0;
    CRGB color = CRGB::Aqua;
    unsigned long lastUpdateTime = 0;

    Worm() {}

    void update(CRGB* leds) {
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - lastUpdateTime;

      if (elapsedTime >= abs(speed)) {
        leds[position] = CRGB::Black; // Turn off the last pixel
        move();
        lastUpdateTime = currentTime;
      }
      leds[position] = color; // Turn on the new pixel

    }

    void move() {
      if (speed < 0) {
        // Move backward
        position = ((position - 1 + NUM_LEDS) % NUM_LEDS);
      } else {
        // Move forward
        position = ((position + 1) % NUM_LEDS);
      }
    }

    void setSpeed(int newSpeed) {
      if (newSpeed >= -127 && newSpeed <= 128) {
        int invertedSpeed = 128 - abs(newSpeed);
        speed = map(invertedSpeed, 0, 128, 1, 200); // Map the inverted speed to the desired range
        if (newSpeed < 0) {
          speed = -speed;
        }
      }
    }

    void setColor(int value){
      //set color with value 0..1023 going through a hue rainbow
      color = mapValueToRainbowColor(value);
    }

    CRGB mapValueToRainbowColor(int value) {
      // Map the input value to the hue range (0-255) in the CHSV color space
      uint8_t hue = map(value, 0, 1023, 0, 255);
      CHSV hsvColor = CHSV(hue, 255, 255); // Full saturation and value for a bright rainbow color
      return hsvColor;
    }
};

class Flash {
  private:
    uint32_t bulkinessTimestamp;
    uint32_t flashTimestamp;
    int flashStart;
    int flashEnd;
    bool active;
    int bulkCount;
    int nextFlashTime;
    bool bulkTriggered;
    int bulkSize;
    int currentFlashDuration;

    int randomize(int value) {
      return value * (100 - storm) / 100 + random(value * storm / 100);
    }

    CRGB randomizeBrightness(CRGB color) {
      uint8_t brght = random(constrain(brightness - 50, 0, 255), brightness); // Random brightness between 90% and 100%
      return color.nscale8_video(brght);
    }

    void triggerFlash(CRGB* leds) {
      bulkinessTimestamp = millis();
      active = true;
      bulkCount++;

      // Spawn a new flash
      flashStart = random(num_leds - length);
      flashEnd = flashStart + randomize(length);

      if(brightness >= 20){
        // Light up the LEDs in the flash area with randomized brightness
        for (int i = flashStart; i <= flashEnd; i++) {
          leds[i] = randomizeBrightness(CRGB::White);
        }        
      }


      // Calculate the next flash time and duration
      nextFlashTime = randomize(bulkFrequency);
      currentFlashDuration = randomize(speed);
      flashTimestamp = millis();
    }

  public:
    int bulkFrequency = 200;
    int speed = 15;
    int num_leds = 300;
    int length = 50;
    int storm = 10;
    int bulkyness = 5;
    uint8_t brightness = 255;

    Flash() : active(false), bulkCount(0), bulkTriggered(false) {
      bulkinessTimestamp = millis();
      flashTimestamp = millis();
      nextFlashTime = randomize(bulkFrequency);
    }

    void triggerBulk() {
      bulkTriggered = true;
      bulkCount = 0;
      bulkSize = randomize(bulkyness);
    }

    void update(CRGB* leds) {
      uint32_t currentTime = millis();

      // If there is an active flash and its duration is over, remove it
      if (active && currentTime - flashTimestamp >= currentFlashDuration) {
        // for (int i = flashStart; i <= flashEnd; i++) {
        //   leds[i] = CRGB::Black;
        // }
        active = false;
      }

      // If a bulk is triggered and enough time has passed, create a new flash in bulk
      if (bulkTriggered && bulkCount < bulkSize && currentTime - bulkinessTimestamp >= nextFlashTime) {
        triggerFlash(leds);
      } else if (bulkCount >= bulkSize) {
        // If the bulk is complete, reset the bulkTriggered flag
        bulkTriggered = false;
      }
    }
};

class BeatFlasher{
  public:
    CRGB highlightColor = CRGB::Red;
    bool highlighted = true;
    int brightness = 255;
    BeatFlasher(){
    }

    void update(CRGB* leds){
      fill_solid(leds, NUM_LEDS, (highlighted?highlightColor:CRGB::Black).nscale8_video(brightness));
    }

    void setBeat(CRGB* leds, int beat){
      highlighted = ((beat + 4)%4) == 0;
    }

    void setColor(int value){
      //set color with value 0..1023 going through a hue rainbow
      highlightColor = mapValueToRainbowColor(value);
    }

    CRGB mapValueToRainbowColor(int value) {
      // Map the input value to the hue range (0-255) in the CHSV color space
      uint8_t hue = map(value, 0, 1023, 0, 255);
      CHSV hsvColor = CHSV(hue, 255, 255); // Full saturation and value for a bright rainbow color
      return hsvColor;
    }

    void setOn(){
      highlighted = true;
    }
};

class TrackedVariable {
  public:
    TrackedVariable(){}
    TrackedVariable(int initialValue): value(initialValue), changed(false) {}

    int getValue() const {
      return value;
    }

    void setValue(int newValue) {
      if (newValue != value) {
        changed = true;
        value = newValue;
      } else {
        changed = false;
      }
    }

    bool hasChanged() const {
      return changed;
    }

  private:
    int value;
    bool changed;
};

int mode = 0; //currently playing animation mode
int modeCount = 4; //number of modes
Strobo strobo;
Worm worm;
Flash flash;
BeatFlasher beatFlash;
uint32_t lastUpdate = millis();

TrackedVariable twinkDens(1);
InputDevice* device = new InputDevice(10); // Start with a BluetoothDevice

int dipSwitchPins[] = {13, 12, 11, 10, 9, 8, 7, 6, 5};
int numPins = sizeof(dipSwitchPins) / sizeof(dipSwitchPins[0]);

//SETUP
void setup() {  
  delay( 3000 ); //safety startup delay
  Serial.begin(9600);
  Wire.begin(9); // Start I2C Bus as Slave with address 9
  Wire.onReceive(I2Creceive); // register event
  //init random seed with noise on analog pin
  randomSeed(analogRead(A3));
  //init strobo
  strobo = Strobo();
  //init flashes
  flash.bulkFrequency = 150;
  flash.storm = 95;
  flash.bulkyness = 6;
  //init twinkles
  initTwinkles();
}

void I2Creceive(int howMany){
  device->receiveEvent(howMany);
}

int lastFlashManual = 0;
int lastTwinkleColor = 0;

//LOOP
void loop()
{
  // //parameters
  // //Twinkle Density 0..8
  // //Twinkle Speed 0..8
  // //Twinkle Color 0..10
  // //flashes 0..255
  // //flashes brightness 0..255
  // //flash manual 0/255
  // //solid amount
  // //solid color
  // //Strobe freq 0..255
  // //Strobe dutycycle

  int mode = device->getValue(1);
  if(mode != 0){
    FastLED.showColor(CRGB(mode,mode,mode));

  } else{
    //Show normal animation mode
    //TWINKLE DENSITY
    TWINKLE_DENSITY = map(device->getValue(2), 0, 255, 0, 8);

    //TWINKLE SPEED
    TWINKLE_SPEED = map(device->getValue(3), 0, 255, 0, 8);

    //TWINKLE COLOR PALETTE
    int twinkleColor = device->getValue(4);
    if(abs(twinkleColor - lastTwinkleColor) >= 10){
      setColorPaletteByIndex(gTargetPalette, map(twinkleColor, 0, 255, 0, 10));  
    }
    lastTwinkleColor = twinkleColor;
    
    //FLASH RATE
    uint32_t flashInterval = map(device->getValue(5), 0, 255, 5000, 10);
    uint32_t currentTime = millis();
    if (currentTime - lastUpdate >= flashInterval && device->getValue(5) != 0) {
      flash.triggerBulk();
      lastUpdate = currentTime;
    }

    //FLASH BRIGHTNESS
    flash.brightness = device->getValue(6);
    //MANUAL FLASH TRIGGER
    int flashManual = device->getValue(7);
    if(lastFlashManual <= 240 && flashManual > 240){
      flash.triggerBulk();
    }
    lastFlashManual = flashManual;

    //update animations
    updateTwinkles();
    flash.update(leds);

    //display pixels
    FastLED.show();
  }
}

void initTwinkles(){
  FastLED.addLeds<LED_TYPE,DATA_PIN,GRB>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip);

  chooseNextColorPalette(gTargetPalette);
}

void updateTwinkles(){  
  EVERY_N_MILLISECONDS( 10 ) {
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 12);
  }
  drawTwinkles( leds);
  // FastLED.show();
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
  &Snow_p,
  &BlueWhite_p,
  &RetroC9_p,
  &RainbowColors_p,
  &FairyLight_p,
  &RedGreenWhite_p,
  &PartyColors_p,
  &RedWhite_p,
  &Holly_p,
  &Ice_p  
};

void setColorPaletteByIndex(CRGBPalette16& pal, uint8_t index) {
  const uint8_t numberOfPalettes = sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);

  // Ensure the index is within the valid range of available palettes
  index = index % numberOfPalettes;

  // Set the color palette by index
  pal = *(ActivePaletteList[index]);
}

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