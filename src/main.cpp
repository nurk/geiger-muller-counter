#include <Arduino.h>

#define PULSE_PIN PIN_PA2
#define LED_PIN PIN_PA5
#define BUZZER_PIN PIN_PA4
#define BUTTON_A_PIN PIN_PA6
#define BUTTON_B_PIN PIN_PA7

volatile boolean pulseDetected    = false;
volatile unsigned long totalCount = 0;
unsigned long secondsElapsed      = 0;

constexpr int LED_ON_TIME      = 10;
constexpr int BUZZER_ON_TIME   = 3; // Keep short for the old school clicky sound
constexpr int BUZZER_FREQUENCY = 2500; // if too thin try lowering (2000), if too dull try increasing (3000)

unsigned long ledOnUntil = 0;
boolean ledIsOn          = false;

// Rolling CPM — 60 one-second buckets
constexpr int CPM_WINDOW                     = 60;
volatile unsigned int cpmBuckets[CPM_WINDOW] = {};
volatile int cpmBucketIndex                  = 0;
unsigned long lastBucketTime                 = 0;

long getRollingCPM() {
    long cpm = 0;
    for (const unsigned int cpmBucket : cpmBuckets) {
        cpm += cpmBucket;
    }
    return cpm;
}

ISR(PORTA_PORT_vect) {
    const byte flags = PORTA.INTFLAGS;
    PORTA.INTFLAGS   = flags; //clear flags
    if (flags & 0x02) {
        pulseDetected = true;
        totalCount++;
        cpmBuckets[cpmBucketIndex]++;
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(PULSE_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);

    PORTA.PIN2CTRL |= PORT_ISC_FALLING_gc;

    CPUINT.LVL1VEC = PORTA_PORT_vect_num; // set highest priority

    lastBucketTime = millis();
}

void loop() {
    // Advance CPM bucket every second
    if (millis() - lastBucketTime >= 1000) {
        lastBucketTime             += 1000;
        cpmBucketIndex             = (cpmBucketIndex + 1) % CPM_WINDOW;
        cpmBuckets[cpmBucketIndex] = 0; // clear the oldest bucket
        secondsElapsed++;

        const bool warmingUp = secondsElapsed < CPM_WINDOW;
        Serial.print("CPM: ");
        Serial.print(getRollingCPM());
        if (warmingUp) {
            Serial.print(" (warming up ");
            Serial.print(secondsElapsed);
            Serial.print("/");
            Serial.print(CPM_WINDOW);
            Serial.print("s)");
        }
        Serial.print(" | Total: ");
        Serial.print(totalCount);
        Serial.print(" | Uptime: ");
        Serial.print(secondsElapsed);
        Serial.println("s");
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
