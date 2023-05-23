// Author: Calmy Jane
// Title: DMX to I2C converter
// Description: Code for arduino that reads DMX bus messages from a DMX shield for Arduino UNO (uses RX/TX pins to read raw dmx data).
// It also reads 9 dipswitches and use them as the starting adress for DMX so the device can be integrated to a DMX setup.
// The messages sent through I2C consist of a channel(8 bit) and a value(8 bit). To keep synchronization a protocol is implemented.
// A pair of channel and value is sent as a packet. A packet consists of 5 bytes:
// First  Byte: 255 (or anything >127, can be used to implement different commands from 127-255)
// Second Byte: 0-127 of the channel
// Third  Byte: 127-255 of the channel
// Fourth Byte: 0-127 of the value
// Fifth  Byte: 127-255 of the value
// This is done because each byte loses 1 bit (the MSB) a high MSB means a new control-byte and the start of a new packet. Low MSB means databyte which can then only have 0-127.
// Example Packet: (255,  1, 0, 127, 13) -> Channel = 1,   Value = 140
// Example Packet: (255, 127, 10, 50, 0) -> Channel = 137, Value = 50

#include <Wire.h>
#include <Conceptinetics.h>

//DMX settings
#define DMX_SLAVE_CHANNELS 100
#define DMX_SLAVE_START_ADDRESS 1
#define RXEN_PIN                2


class DipSwitches {
  public:
    // This class represents a row of dip switches connected to digital pins of an Arduino.
    // If you have e.g. 8 dip switches, the value can be 0-255
    int* pins; // Pin numbers of the dip switches, this array should be variable size depending on the number of input pins passed to the constructor
    int numPins;
    int currValue = 0; // Value indicated by the dip switches in binary. First pin is least significant bit
    // Add a callback function that is called whenever the value of the dip switches has changed
    typedef void (*CallbackFunction)(int);
    CallbackFunction callback;

    DipSwitches(int* pins, CallbackFunction callback) {
      this->pins = pins;
      this->numPins = sizeof(pins);
      this->callback = callback;
      for (int i = 0; i < numPins; i++) {
        pinMode(pins[i], INPUT_PULLUP);
      }
      Update();
    }

    void Update() {
      int newValue = 0;
      for (int i = 0; i < numPins; i++) {
        newValue |= (digitalRead(pins[i]) == LOW) << i;
      }

      if (newValue != currValue) {
        currValue = newValue;
        if (callback) {
          callback(currValue);
        }
      }
    }
};

void dipSwitchChanged(int newValue){
}

DMX_Slave dmx_slave (DMX_SLAVE_CHANNELS);
int* lastValues;
DipSwitches dips(new int[9]{5,12,11,10,9,8,7,6,A0}, dipSwitchChanged);


void setup() {
  Wire.begin();
  dips.Update();//update dip switches so curr set adress can be set
  dmx_slave.enable (); 
  dmx_slave.setStartAddress (dips.currValue);
  // dmx_slave.onReceiveComplete(onDmxReceive);
  pinMode(13, OUTPUT);
  lastValues = new int[DMX_SLAVE_CHANNELS](0);
}

// //This was a try to make it non-polling, but somehow the callback is never called
// void onDmxReceive(unsigned short channel){
//   //write dmx channel to I2c on value change
//   sendI2C(channel, dmx_slave.getChannelValue(channel));
// }

void sendI2C(int channel, int value){
    uint8_t* msg = buildMessage(channel, value);
    Wire.beginTransmission(9); // Start communication with Slave (address 9)
    for(int j=0; j<5; j++){
      Wire.write(msg[j]); // Send bytes
    }
    Wire.endTransmission(); // End transmission  
    delete(msg);
}

void loop() {
  for(int i=0; i<DMX_SLAVE_CHANNELS; i++){
    int val = dmx_slave.getChannelValue(i+1);
    if(val != lastValues[i]){
      sendI2C(i+1, dmx_slave.getChannelValue(i+1));
    }
    lastValues[i] = val;
  }
  digitalWrite(13, dmx_slave.getChannelValue(1) < 50);
}

uint8_t* buildMessage(int channel, int value) {
    // Prepare the array
    uint8_t* message = new uint8_t[5];

    // The first element is 255
    message[0] = (byte)255;

    // The second and third elements represent the 'channel'
    // Split the 8-bit integer 'channel' into two 7-bit integers
    message[1] = (byte)(channel <= 127 ? channel : 127); // Get the first half
    message[2] = (byte)(channel <= 127 ? 0 : channel - 127); // Get the second half

    // The fourth and fifth elements represent the 'value'
    // Split the 8-bit integer 'value' into two 7-bit integers
    message[3] = (byte)(value <= 127 ? value : 127); // Get the first half
    message[4] = (byte)(value <= 127 ? 0 : value - 127); // Get the second half

    return message;
}