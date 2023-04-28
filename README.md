# Cloudyo
A NeoPixel LED Controller made for cloud lamps



This code reads midi on Rx pin of the Arduino, reads 2 Potentiometers (right, left), 3 buttons (right, middle, left) and controls 4 LEDs.
When press&hold middle button you switch through 4 animation modes:
1. Thunderstorm - Left Poti controls Flash-Intensity (brightness and rate), right poti controls background colorpalette. Middle Button triggers Flash immediately. Right button changes background warp-speed. Left Button changes background density.
2. Beat Flasher - Left Poti changes flash color, Right poti changes flash brightness - This mode is constant single color if no Midi is connected. If a midi clock is recieved, it will blink with the midi clock.
3. Strobolicious - Left Poti changes flash-rate, right poti changes color
4. Wormhole - Left Poti changes move speed, right poti changes color, middle button draws some colors once for the pixel to eat up
