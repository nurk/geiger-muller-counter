#ifndef GEIGER_MULLER_COUNTER_DISPLAYCONTROLLER_H
#define GEIGER_MULLER_COUNTER_DISPLAYCONTROLLER_H

#include <Adafruit_SSD1306.h>
#include <GlobalConstants.h>

class DisplayController {
public:
    explicit DisplayController(Adafruit_SSD1306& display);

    void begin() const;
    void update(uint32_t secondsElapsed,
                const volatile uint32_t& totalCount,
                const volatile uint16_t (&cpmBuckets)[CPM_WINDOW],
                const volatile uint16_t& cpmBucketIndex);

private:
    void drawStats(uint32_t secondsElapsed,
                   uint32_t totalCount,
                   const uint16_t (&cpmBuckets)[CPM_WINDOW],
                   uint16_t bucketMax) const;

    uint16_t drawGraph(const uint16_t (&cpmBuckets)[CPM_WINDOW], // NOLINT(*-use-nodiscard)
                       uint16_t cpmBucketIndex) const;

    static int32_t getRollingCPM(const uint16_t (&cpmBuckets)[CPM_WINDOW]);
    static void formatUptime(uint32_t totalSeconds, char* buf, size_t len);

    Adafruit_SSD1306& display;

    static constexpr uint8_t DISPLAY_WIDTH = 128;
    static constexpr uint8_t GRAPH_HEIGHT  = 28; // height of CPM graph area
    static constexpr uint8_t GRAPH_Y       = 36; // top-y of graph area
    static constexpr uint16_t UPDATE_MS    = 250;

    uint32_t lastUpdateMillis = 0;
};

#endif //GEIGER_MULLER_COUNTER_DISPLAYCONTROLLER_H
