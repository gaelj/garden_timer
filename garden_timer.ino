
/*
This Arduino sketch triggers a relay on and off periodically.
The output of a light sensing resistor in a voltage divisor further
conditions setting the relay to ON.

RGB LED configuration

Show mode: auto / forced on / forced off
Show relay state: on / off
Show isDay state

--- LED color code ---
auto on     yellow
auto off    blue
forced on   green
forced off  red
isNight     violet


(c) Gaël JAMES, 2019
*/

// External libraries
#include <FastLED.h>

// Time constants
const unsigned long SECOND = 1000;
const unsigned long MINUTE = 60 * SECOND;
const unsigned long HOUR = 60 * MINUTE;

// ************** USER CONFIGURATION - START **************
// Timer configuration
const unsigned long ON_DURATION = 5 * SECOND;
const unsigned long OFF_DURATION = 1 * HOUR - ON_DURATION;

// Illumination sensor value and time hysteresis configuration
// The more light there is, the lower the sensor value
const int DAY_THRESHOLD_LOW = 750;                          // light sensor threshold
const int DAY_THRESHOLD_HIGH = 950;                         // the measured value decreases when luminosity increases
const int NB_OF_IDENTICAL_CONSECUTIVE_ILLUM_MEAS = 5000;    // How many consecutive measurements (x2ms loop time) must agree before changing day/night state

// LED brightness
const byte BRIGHTNESS_DAY = 128;
const byte BRIGHTNESS_NIGHT = 96;
// ************** USER CONFIGURATION - END **************

// Pinout configuration
const int lightSensorPin = A0; // illumination sensor
const byte relayPin1 = 5;      // 220V relay
const byte relayPin2 = 6;      // 220V relay
const byte buttonPin = 2;      // push button (interrupt input)
const byte LED_PIN = 4;
const byte NUM_LEDS = 1;
const uint8_t BUTTON_DEBOUNCE_TIME = 500; // ms

// RGB LED configuration
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette;
TBlendType currentBlending;
uint8_t brightness = 0;

// State constants, used to represent both current mode (auto/off/on) and desired relay state change (off/on/noChange)
const byte state_auto = 0;
const byte state_on = 1;
const byte state_off = 2;
const byte state_noChange = 3;

// Global state variables. We use globals instead of passing arguments in function calls for memory usage optimisation reasons
byte isDay = false;                       // is it now day or night
volatile byte previousMode = state_off;   // previous mode (auto, on or off)
volatile byte currentMode = state_auto;   // current mode (auto, on or off)
byte previousTriggerType = LOW;           // previous relay position
byte currentTriggerType = LOW;            // current relay position
int identical_consecutive_meas_count = 0; // how many identical consecutive sensor readouts are needed before toggling the day/night state

/**
 * Arduino setup function, called once right after power on
 */
void setup()
{
    Serial.begin(9600);
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(lightSensorPin, INPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonPin), onButtonPressed, FALLING);

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS_DAY);
    currentPalette = RainbowColors_p;
    currentBlending = LINEARBLEND;
    FillLEDsFromPaletteColors(0);
    FastLED.show();
}

/**
 * Arduino loop function, called periodically
 */
void loop()
{
    DecayLedBrightness();
    checkForIlluminationChange();
    ProcessTimerElapsed();
    ProcessModeChange();
    SetLedColor();
    delay(2);
}

/**
 * React on button-pressed HW interrupts, with debounce
 */
void onButtonPressed()
{
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();

    // If interrupts come too fast, assume it's a bounce and ignore
    if (interrupt_time - last_interrupt_time > BUTTON_DEBOUNCE_TIME)
        currentMode = (currentMode + 1) % state_noChange;
    last_interrupt_time = interrupt_time;
}

/**
 * Toggle the "isDay" global variable when the illumination change has been sufficiently strong and long to be considered valid
 */
void checkForIlluminationChange()
{
    int sensorValue = analogRead(lightSensorPin);
    // Serial.println(sensorValue);

    if (sensorValue < DAY_THRESHOLD_LOW)
    {
        if (!isDay) {
            if (identical_consecutive_meas_count == 0)
                Serial.println("Sensor detected Day");
            identical_consecutive_meas_count++;
            if (identical_consecutive_meas_count == NB_OF_IDENTICAL_CONSECUTIVE_ILLUM_MEAS)
            {
                Serial.println("Sensor confirmed Day");
                identical_consecutive_meas_count = 0;
                isDay = true;
            }
        } else if (identical_consecutive_meas_count > 0) {
            Serial.println("Sensor to night invalidated");
            identical_consecutive_meas_count = 0;
        }
    }
    else if (sensorValue > DAY_THRESHOLD_HIGH)
    {
        if (isDay) {
            if (identical_consecutive_meas_count == 0)
                Serial.println("Sensor detected Night");
            identical_consecutive_meas_count++;
            if (identical_consecutive_meas_count == NB_OF_IDENTICAL_CONSECUTIVE_ILLUM_MEAS)
            {
                Serial.println("Sensor confirmed Night");
                identical_consecutive_meas_count = 0;
                isDay = false;
            }
        } else if (identical_consecutive_meas_count > 0) {
            Serial.println("Sensor to day invalidated");
            identical_consecutive_meas_count = 0;
        }
    }
}

/**
 * If a relay state change has been requested (by setting a new value to currentTriggerType),
 * toggle the relays and set the LED brightness to max
 */
void setRelay()
{
    if (previousTriggerType == currentTriggerType)
        return;
    previousTriggerType = currentTriggerType;
    Serial.print("Trigger ");
    if (currentTriggerType == LOW)
        Serial.println("Relays OFF");
    else
        Serial.println("Relays ON");
    digitalWrite(relayPin1, currentTriggerType);
    digitalWrite(relayPin2, currentTriggerType);
    brightness = isDay ? BRIGHTNESS_DAY : BRIGHTNESS_NIGHT;
    FastLED.setBrightness(brightness);
}

/**
 * Decays the LED brightness with time, in auto mode only
 */
void DecayLedBrightness()
{
    static unsigned long nextBlink = 0;
    if (currentMode == state_auto && millis() > nextBlink)
    {
        if (brightness > 0)
            brightness--;
        FastLED.setBrightness(brightness);
        nextBlink = millis() + 50;
    }
}

/**
 * Toggle the relays to the desired new state when the timer elapses
 */
void ProcessTimerElapsed()
{
    static unsigned long nextTriggerTime = 0;
    bool timerIsElapsed = nextTriggerTime < millis();

    if (timerIsElapsed)
    {
        Serial.println("TIMER ELAPSED");
        if (previousTriggerType == HIGH)
            nextTriggerTime = millis() + OFF_DURATION;
        else
            nextTriggerTime = millis() + ON_DURATION;

        if (currentMode == state_auto)
        {
            currentTriggerType = !previousTriggerType;
            if (currentTriggerType == HIGH && !isDay)
                currentTriggerType = LOW;

            setRelay();
        }
    }
}

/**
 * When a mode change has been initiated (by pressing the button, which sets a new value to currentMode),
 * set the relay value in case of forced mode, and set the LED brightness to max, then toggle the relays
 */
void ProcessModeChange()
{
    if (previousMode != currentMode)
    {
        previousMode = currentMode;
        Serial.print("MODE CHANGED TO: ");
        switch (currentMode)
        {
        case state_auto:
            Serial.println("AUTO");
            break;

        case state_on:
            Serial.println("FORCED ON");
            currentTriggerType = HIGH;
            break;

        case state_off:
            Serial.println("FORCED OFF");
            currentTriggerType = LOW;
            break;
        }

        brightness = isDay ? BRIGHTNESS_DAY : BRIGHTNESS_NIGHT;
        FastLED.setBrightness(brightness);
        setRelay();
    }
}

/**
 * Set the appropriate LED color, according the the current mode
 */
void SetLedColor()
{
    uint8_t colorIndex = 0; // white
    if (currentMode == state_auto)
    {
        if (!isDay)
            colorIndex = 128; // violet
        else {
            if (currentTriggerType == HIGH)
                colorIndex = 72; // yellow
            else
                colorIndex = 176; // blue
        }
    }
    else
    {
        if (currentTriggerType == HIGH)
            colorIndex = 20; // green
        else
            colorIndex = 96; // red
    }

    FillLEDsFromPaletteColors(colorIndex);
    FastLED.show();
}

/**
 * Apply the colors to the LED array
 */
void FillLEDsFromPaletteColors(uint8_t colorIndex)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 1;
    }
}
