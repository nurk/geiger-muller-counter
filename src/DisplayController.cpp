#include <DisplayController.h>
#include <Adafruit_GFX.h>
#include <util/atomic.h>
#include <string.h> // NOLINT(*-deprecated-headers) — AVR-libc does not provide <cstring>

DisplayController::DisplayController(Adafruit_SSD1306& display_)
    : display(display_) {
}

void DisplayController::begin() const {
    display.clearDisplay();
    display.display();
}

void DisplayController::update(const unsigned long secondsElapsed,
                               const unsigned long totalCount,
                               volatile unsigned int (&cpmBuckets)[CPM_WINDOW],
                               const int cpmBucketIndex) {
    if (millis() - lastUpdateMillis < UPDATE_MS) {
        return;
    }

    lastUpdateMillis = millis();

    // static: lives in BSS (static RAM), not the stack — saves 600 bytes of stack
    static unsigned int snapshotCpmBuckets[CPM_WINDOW];
    unsigned long snapshotSecondsElapsed = 0;
    unsigned long snapshotTotalCount     = 0;
    int snapshotCpmBucketIndex           = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        snapshotSecondsElapsed = secondsElapsed;
        snapshotTotalCount     = totalCount;
        snapshotCpmBucketIndex = cpmBucketIndex;
        // memcpy is much faster than a manual loop — minimizes interrupt-disabled time
        memcpy(snapshotCpmBuckets,
               const_cast<unsigned int*>(cpmBuckets),
               sizeof(snapshotCpmBuckets));
    }

    display.clearDisplay();

    const unsigned int bucketMax = drawGraph(snapshotCpmBuckets, snapshotCpmBucketIndex);
    drawStats(snapshotSecondsElapsed, snapshotTotalCount, snapshotCpmBuckets, bucketMax);

    display.display();
}

// ── private ────────────────────────────────────────────────────────────────

void DisplayController::drawStats(const unsigned long secondsElapsed,
                                  const unsigned long totalCount,
                                  const unsigned int (&cpmBuckets)[CPM_WINDOW],
                                  const unsigned int bucketMax) const {
    const long cpm       = getRollingCPM(cpmBuckets);
    const bool warmingUp = secondsElapsed < CPM_WINDOW;

    char buf[32];

    // Labels are padded to 6 characters so the colon always lands at x=36 (6×6 px)
    // and all values start at VALUE_X = 42, keeping columns visually aligned.
    // Adafruit_GFX size-1 font: each character is 6 px wide.
    static constexpr int VALUE_X = 48; // x pixel where values start (8 chars × 6 px)

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Line 1: Total count ──────────────────────────────  y = 0
    display.setCursor(0, 0);
    display.print(F("Total : "));
    display.setCursor(VALUE_X, 0);
    display.print(totalCount);

    // ── Line 2: Uptime ───────────────────────────────────  y = 10
    formatUptime(secondsElapsed, buf, sizeof(buf));
    display.setCursor(0, 10);
    display.print(F("Up    : "));
    display.setCursor(VALUE_X, 10);
    display.print(buf);

    // ── Line 3: CPM ──────────────────────────────────────  y = 20
    display.setCursor(0, 20);
    display.print(F("CPM   : "));
    display.setCursor(VALUE_X, 20);
    display.print(cpm);
    if (warmingUp) {
        display.print(F("*"));
    }

    // ── Scale label — right-aligned on the CPM line ───────────────────────────
    // ^N = pulse count of the tallest 1-second bucket (graph full-scale reference)
    char scaleBuf[6];
    snprintf(scaleBuf, sizeof(scaleBuf), "^%u", bucketMax);
    const int labelW = static_cast<int>(strlen(scaleBuf)) * 6;
    display.setCursor(DISPLAY_WIDTH - labelW, 20);
    display.print(scaleBuf);

    // ── Divider above graph ───────────────────────────────  y = 43
    display.drawFastHLine(0, GRAPH_Y - 3, DISPLAY_WIDTH, SSD1306_WHITE);
}

unsigned int DisplayController::drawGraph(const unsigned int (&cpmBuckets)[CPM_WINDOW],
                                          const int cpmBucketIndex) const {
    // Collapse CPM_WINDOW buckets into DISPLAY_W columns.
    // Each column aggregates (CPM_WINDOW / DISPLAY_W) consecutive seconds.
    // With CPM_WINDOW=300 and DISPLAY_W=128: ~2.34 s per column — we use
    // integer bucketsPerCol and spread the remainder evenly via the index mapping.

    // Find per-column sums and the max for auto-scaling.
    // We map column i → bucket range [start, end).
    unsigned int colMax    = 1; // avoid division by zero in bar height calculation
    unsigned int bucketMax = 0; // peak single-bucket pulse count → scale label
    // static: lives in BSS (static RAM), not the stack — saves 256 bytes of stack
    static unsigned int colSums[DISPLAY_WIDTH];

    for (int col = 0; col < DISPLAY_WIDTH; col++) {
        // Map column to bucket indices (oldest → newest, left → right)
        const int bucketStart = static_cast<int>((col * static_cast<long>(CPM_WINDOW)) / DISPLAY_WIDTH);
        int bucketEnd         = static_cast<int>(((col + 1) * static_cast<long>(CPM_WINDOW)) / DISPLAY_WIDTH);
        if (bucketEnd == bucketStart) {
            bucketEnd = bucketStart + 1; // at least 1
        }

        unsigned int sum = 0;
        for (int bucketIndex = bucketStart; bucketIndex < bucketEnd; bucketIndex++) {
            const int idx                   = (cpmBucketIndex + 1 + bucketIndex) % CPM_WINDOW;
            const unsigned int bucketPulses = cpmBuckets[idx];
            sum                             += bucketPulses;
            if (bucketPulses > bucketMax) {
                bucketMax = bucketPulses;
            }
        }
        colSums[col] = sum;
        if (sum > colMax) {
            colMax = sum;
        }
    }

    for (int col = 0; col < DISPLAY_WIDTH; col++) {
        int barHeight = (static_cast<int>(colSums[col]) * GRAPH_HEIGHT) / static_cast<int>(colMax);
        if (barHeight < 1 && colSums[col] > 0u) {
            barHeight = 1;
        }
        int y = GRAPH_Y + (GRAPH_HEIGHT - barHeight);
        display.drawFastVLine(col, y, barHeight, SSD1306_WHITE);
    }

    return bucketMax;
}

long DisplayController::getRollingCPM(const unsigned int (&cpmBuckets)[CPM_WINDOW]) {
    long counts = 0;
    for (const unsigned int cpmBucket : cpmBuckets) {
        counts += cpmBucket;
    }
    // Normalize: window holds CPM_WINDOW seconds of data, CPM = counts/min
    return (counts * 60L) / CPM_WINDOW;
}

// ReSharper disable once CppDFAConstantParameter
void DisplayController::formatUptime(const unsigned long totalSeconds, char* buf, const size_t len) {
    const unsigned long days    = totalSeconds / 86400UL;
    const unsigned long hours   = (totalSeconds % 86400UL) / 3600UL;
    const unsigned long minutes = (totalSeconds % 3600UL) / 60UL;
    const unsigned long seconds = totalSeconds % 60UL;
    if (days > 0) {
        snprintf(buf, len, "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    } else {
        snprintf(buf, len, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    }
}
