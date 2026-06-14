#include <DisplayController.h>
#include <Adafruit_GFX.h>
#include <util/atomic.h>
#include <string.h> // NOLINT(*-deprecated-headers) — AVR-libc does not provide <cstring>

DisplayController::DisplayController(Adafruit_SSD1306& display)
    : display(display) {
}

void DisplayController::begin() const {
    display.clearDisplay();
    display.display();
}

void DisplayController::update(const uint32_t secondsElapsed,
                               const volatile uint32_t& totalCount,
                               const volatile uint16_t (&cpmBuckets)[CPM_WINDOW],
                               const volatile uint16_t& cpmBucketIndex) {
    if (millis() - lastUpdateMillis < UPDATE_MS) {
        return;
    }

    lastUpdateMillis = millis();

    // static: lives in BSS (static RAM), not the stack — saves 600 bytes of stack
    static uint16_t snapshotCpmBuckets[CPM_WINDOW];
    uint32_t snapshotSecondsElapsed = 0;
    uint32_t snapshotTotalCount     = 0;
    uint16_t snapshotCpmBucketIndex = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        snapshotSecondsElapsed = secondsElapsed;
        snapshotTotalCount     = totalCount;
        snapshotCpmBucketIndex = cpmBucketIndex;
        // memcpy is much faster than a manual loop — minimizes interrupt-disabled time
        memcpy(snapshotCpmBuckets,
               const_cast<uint16_t*>(cpmBuckets),
               sizeof(snapshotCpmBuckets));
    }

    display.clearDisplay();

    const uint16_t bucketMax = drawGraph(snapshotCpmBuckets, snapshotCpmBucketIndex);
    drawStats(snapshotSecondsElapsed, snapshotTotalCount, snapshotCpmBuckets, bucketMax);

    display.display();
}

// ── private ────────────────────────────────────────────────────────────────

void DisplayController::drawStats(const uint32_t secondsElapsed,
                                  const uint32_t totalCount,
                                  const uint16_t (&cpmBuckets)[CPM_WINDOW],
                                  const uint16_t bucketMax) const {
    const int32_t cpm    = getRollingCPM(cpmBuckets, secondsElapsed);
    const bool warmingUp = secondsElapsed < CPM_WINDOW;

    char buf[32];

    // Labels are padded to 6 characters so the colon always lands at x=36 (6×6 px)
    // and all values start at VALUE_X = 42, keeping columns visually aligned.
    // Adafruit_GFX size-1 font: each character is 6 px wide.
    static constexpr int16_t VALUE_X = 48; // x pixel where values start (8 chars × 6 px)

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
    const auto labelW = static_cast<int16_t>(strlen(scaleBuf) * 6U);
    display.setCursor(DISPLAY_WIDTH - labelW, 20);
    display.print(scaleBuf);

    // ── Divider above graph ───────────────────────────────  y = 43
    display.drawFastHLine(0, GRAPH_Y - 3, DISPLAY_WIDTH, SSD1306_WHITE);
}

uint16_t DisplayController::drawGraph(const uint16_t (&cpmBuckets)[CPM_WINDOW],
                                      const uint16_t cpmBucketIndex) const {
    // Collapse CPM_WINDOW buckets into DISPLAY_W columns.
    // Each column aggregates (CPM_WINDOW / DISPLAY_W) consecutive seconds.
    // With CPM_WINDOW=300 and DISPLAY_W=128: ~2.34 s per column.

    // Find per-column sums and the max average for auto-scaling.
    // We scale averages by 12 to maintain exact integer precision (since denominators are 2 or 3).
    uint16_t colMax    = 12; // default to 1 CPS (12/12) to avoid division by zero and handle low rates
    uint16_t bucketMax = 0; // peak single-bucket pulse count → scale label
    // static: lives in BSS (static RAM), not the stack — saves 256 bytes of stack
    static uint16_t colSums[DISPLAY_WIDTH];

    for (int col = 0; col < DISPLAY_WIDTH; col++) {
        // Map column to bucket indices (oldest → newest, left → right)
        const int bucketStart = static_cast<int>((col * static_cast<long>(CPM_WINDOW)) / DISPLAY_WIDTH);
        int bucketEnd         = static_cast<int>(((col + 1) * static_cast<long>(CPM_WINDOW)) / DISPLAY_WIDTH);
        if (bucketEnd == bucketStart) {
            bucketEnd = bucketStart + 1; // at least 1
        }

        uint16_t sum = 0;
        for (int bucketIndex = bucketStart; bucketIndex < bucketEnd; bucketIndex++) {
            const uint16_t idx          = (cpmBucketIndex + 1 + bucketIndex) % CPM_WINDOW;
            const uint16_t bucketPulses = cpmBuckets[idx];
            sum                         += bucketPulses;
            if (bucketPulses > bucketMax) {
                bucketMax = bucketPulses;
            }
        }

        const int width = bucketEnd - bucketStart;
        // Scale by 12 for exact average (12 is divisible by both 2 and 3)
        const auto avgScaled = static_cast<uint16_t>((static_cast<uint32_t>(sum) * 12UL) / width);
        colSums[col]         = avgScaled;

        if (avgScaled > colMax) {
            colMax = avgScaled;
        }
    }

    // Use bucketMax as the scale divisor to match the label
    const uint16_t divisor = (bucketMax > 0) ? bucketMax : 12;
    for (int col = 0; col < DISPLAY_WIDTH; col++) {
        auto barHeight = static_cast<int16_t>((static_cast<uint32_t>(colSums[col]) * GRAPH_HEIGHT) / (static_cast<
            uint32_t>(divisor) * 12UL));
        if (barHeight < 1 && colSums[col] > 0u) {
            barHeight = 1;
        }
        const auto y = static_cast<int16_t>(GRAPH_Y + (GRAPH_HEIGHT - barHeight));
        display.drawFastVLine(col, y, barHeight, SSD1306_WHITE);
    }

    return bucketMax;
}

int32_t DisplayController::getRollingCPM(const uint16_t (&cpmBuckets)[CPM_WINDOW], const uint32_t secondsElapsed) {
    int32_t counts = 0;
    for (const uint16_t cpmBucket : cpmBuckets) {
        counts += cpmBucket;
    }
    if (secondsElapsed == 0) {
        return 0;
    }
    const uint32_t divisor = (secondsElapsed < CPM_WINDOW) ? secondsElapsed : CPM_WINDOW;
    // Normalize: window holds divisor seconds of data, CPM = counts * 60 / divisor
    return static_cast<int32_t>(counts * 60L / divisor);
}

// ReSharper disable once CppDFAConstantParameter
void DisplayController::formatUptime(const uint32_t totalSeconds, char* buf, const size_t len) {
    const uint32_t days    = totalSeconds / 86400UL;
    const uint32_t hours   = (totalSeconds % 86400UL) / 3600UL;
    const uint32_t minutes = (totalSeconds % 3600UL) / 60UL;
    const uint32_t seconds = totalSeconds % 60UL;
    if (days > 0) {
        snprintf(buf, len, "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    } else {
        snprintf(buf, len, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    }
}
