#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial btSerial = SoftwareSerial(3,4);
int* buffer;
int* channels;
bool* changed;
int channelCount = 100;
int bufferIndex = 0;

void setup() {
  // put your setup code here, to run once:
  btSerial.begin(9600);
  Serial.begin(9600);
  Wire.begin(); // Start I2C Bus as Master
  buffer = new int[5]();
  channels = new int[channelCount]();
  changed = new bool[channelCount]();

}

void loop() {
  // put your main code here, to run repeatedly:
  readSerial();
  for(int i=0; i<channelCount; i++){
    if(changed[i]){
      Wire.beginTransmission(9); // Start communication with Slave (address 9)
      uint8_t* msg = buildMessage(i + 1, channels[i]);
      for(int j=0; j<5; j++){
        Wire.write(msg[j]); // Send bytes
      }
      delete(msg);  
      changed[i] = false;    
      Wire.endTransmission(); // End transmission
    }
  }
  Serial.print(channels[0]);
  Serial.println();
}

void readSerial(){
    while (btSerial.available()) {
      uint8_t incoming = btSerial.read();
      
      // Check if a new message should start
      if (incoming > 127) {
        bufferIndex = 0;
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
          changed[channelNum] = true;
        }
        bufferIndex = 0;  // Reset the buffer index for the next message
      }
    }
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
