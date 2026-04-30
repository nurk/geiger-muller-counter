#ifndef GEIGER_MULLER_COUNTER_DISPLAYCONTROLLER_H
#define GEIGER_MULLER_COUNTER_DISPLAYCONTROLLER_H

#include <Adafruit_SSD1306.h>
#include <GlobalConstants.h>

class DisplayController {
public:
    explicit DisplayController(Adafruit_SSD1306& displayIn);

    void begin() const;
    void update(unsigned long secondsElapsed,
                unsigned long totalCount,
                volatile unsigned int (&cpmBuckets)[CPM_WINDOW],
                int cpmBucketIndex);

private:
    Adafruit_SSD1306& display;

    static constexpr int DISPLAY_WIDTH      = 128;
    static constexpr int GRAPH_HEIGHT       = 18; // height of CPM graph area
    static constexpr int GRAPH_Y            = 46; // top-y of graph area
    static constexpr unsigned int UPDATE_MS = 250;

    unsigned long lastUpdateMillis = 0;

    void drawStats(unsigned long secondsElapsed,
                   unsigned long totalCount,
                   const unsigned int (&cpmBuckets)[CPM_WINDOW]);

    void drawGraph(const unsigned int (&cpmBuckets)[CPM_WINDOW],
                   int cpmBucketIndex) const;

    static long getRollingCPM(const unsigned int (&cpmBuckets)[CPM_WINDOW]);
    static void formatUptime(unsigned long totalSeconds, char* buf, size_t len);
};

#endif //GEIGER_MULLER_COUNTER_DISPLAYCONTROLLER_H
