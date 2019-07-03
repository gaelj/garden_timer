# Garden Timer

The goal of this project is to periodically trigger a water pump for a short time, only during day time.
To achieve this, we used an Arduino Nano, a dual relay board, an illumination sensor + resistor board, a push button, WS2811 RGB LED(s), and a small prototype board to wire eveything up.
The RGB LED informs about mode and mode changes.
The push-button cycles through modes: auto, forced on, forced off.

The pinout is provided in the configuration section of the sketch and can be adapted, with the following guidelines:
- use an input with INTerrupt for the button
- use an analog input for the sensor

Further cabling instructions:
- use a series 1k resistor between the digital output pin and the RGB LED input
- connect the other pin of the switch to GND
- connect +5V power and GND to the illumniation sensor module, the LED(s) and the relay board

Be carefull that RGB LEDs consume quite a lot of current. Take care not to overload the PSU pins of the Arduino when daisy-chaining multiple LEDs and using the Arduino's ext +5V pin. Install a ~µF PSU filtering capacitor (between +5V and GND) when using multiple LEDs.

Set user-defined configuration values as desired in the code, and compile & upload to the board.
The only external dependency is the standard FastLED library.
