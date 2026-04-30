#include <Arduino.h>
#include <Wire.h>
#include <JC_Button.h>
#include <Adafruit_SSD1306.h>
#include <DisplayController.h>
#include <GlobalConstants.h>

#define PULSE_PIN PIN_PA2
#define LED_PIN PIN_PA5
#define BUZZER_PIN PIN_PA4
#define BUTTON_A_PIN PIN_PA6
#define BUTTON_B_PIN PIN_PA7

volatile bool pulseDetected       = false;
volatile unsigned long totalCount = 0;
unsigned long secondsElapsed      = 0;

constexpr int LED_ON_TIME      = 10;
constexpr int BUZZER_ON_TIME   = 3; // Keep short for the old school clicky sound
constexpr int BUZZER_FREQUENCY = 2500; // if too thin try lowering (2000), if too dull try increasing (3000)

unsigned long ledOnUntil = 0;
bool ledIsOn             = false;

volatile unsigned int cpmBuckets[CPM_WINDOW] = {};
volatile int cpmBucketIndex                  = 0;
unsigned long lastBucketTime                 = 0;

Button buttonA(BUTTON_A_PIN);
Button buttonB(BUTTON_B_PIN);

Adafruit_SSD1306 display(128, 64, &Wire, -1); // NOLINT(*-interfaces-global-init)
DisplayController displayController(display);

ISR(PORTA_PORT_vect) {
    const byte flags = PORTA.INTFLAGS;
    PORTA.INTFLAGS   = flags; //clear flags
    if (flags & 0x02) {
        pulseDetected = true;
        totalCount++;
        cpmBuckets[cpmBucketIndex]++;
    }
}

void handleUserInputs() {
    buttonA.read();
    buttonB.read();
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(400000UL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
    }

    pinMode(PULSE_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);

    PORTA.PIN2CTRL |= PORT_ISC_FALLING_gc;

    CPUINT.LVL1VEC = PORTA_PORT_vect_num; // set highest priority

    buttonA.begin();
    buttonB.begin();

    displayController.begin();

    lastBucketTime = millis();
}

void loop() {
    // handleUserInputs();
    displayController.update(secondsElapsed, totalCount, cpmBuckets, cpmBucketIndex);

    // Advance CPM bucket every second
    if (millis() - lastBucketTime >= 1000) {
        lastBucketTime             += 1000;
        cpmBucketIndex             = (cpmBucketIndex + 1) % CPM_WINDOW;
        cpmBuckets[cpmBucketIndex] = 0; // clear the oldest bucket
        secondsElapsed++;
    }

    if (ledIsOn && millis() > ledOnUntil) {
        digitalWriteFast(LED_PIN, LOW);
        ledIsOn = false;
    }

    if (pulseDetected) {
#ifdef DEBUG
        Serial.print("Pulse detected! Total count: ");
        Serial.println(totalCount);
#endif
        pulseDetected = false;

        digitalWriteFast(LED_PIN, HIGH);
        ledIsOn    = true;
        ledOnUntil = millis() + LED_ON_TIME;

        tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_ON_TIME);
    }
}
