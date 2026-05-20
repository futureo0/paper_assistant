#pragma once
#include <Arduino.h>

namespace touch_ft6336 {

struct Point {
    uint16_t x;
    uint16_t y;
};

bool begin();
bool get_touch(Point& point);
bool consume_cartoon_tap(Point& point);

}  // namespace touch_ft6336
