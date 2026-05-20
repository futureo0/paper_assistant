#include "touch_ft6336.h"

#include <Wire.h>

namespace touch_ft6336 {

static constexpr uint8_t FT6336_ADDR = 0x38;
static constexpr int TOUCH_RST = 7;
static constexpr int SCR_W = 200;
static constexpr int SCR_H = 200;
static constexpr int CARTOON_X = 10;
static constexpr int CARTOON_Y = 15;
static constexpr int CARTOON_W = 180;
static constexpr int CARTOON_H = 180;
static constexpr uint32_t TAP_DEBOUNCE_MS = 700;
static constexpr uint32_t TOUCH_POLL_MS = 20;
static constexpr uint8_t BEGIN_RETRY_COUNT = 10;
static constexpr uint32_t BEGIN_RETRY_DELAY_MS = 50;

static bool g_ready = false;
static bool g_was_touched = false;
static uint32_t g_last_tap_ms = 0;
static uint32_t g_last_poll_ms = 0;

static bool read_reg(uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    return Wire.requestFrom(FT6336_ADDR, (uint8_t)len) == len && [&]() {
        for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
        return true;
    }();
}

static bool read_touch_raw(uint8_t& count, Point& point) {
    uint8_t raw_count = 0;
    if (!read_reg(0x02, &raw_count, 1)) return false;
    count = raw_count & 0x0F;
    if (count == 0) return true;

    uint8_t buf[4] = {};
    if (!read_reg(0x03, buf, sizeof(buf))) return false;
    uint16_t x = (((uint16_t)buf[0] & 0x0F) << 8) | buf[1];
    uint16_t y = (((uint16_t)buf[2] & 0x0F) << 8) | buf[3];
    if (x >= SCR_W) x = SCR_W - 1;
    if (y >= SCR_H) y = SCR_H - 1;
    point = {x, y};
    return true;
}

static bool point_in_cartoon(const Point& p) {
    return p.x >= CARTOON_X && p.x < CARTOON_X + CARTOON_W &&
           p.y >= CARTOON_Y && p.y < CARTOON_Y + CARTOON_H;
}

bool begin() {
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, HIGH);
    delay(100);
    digitalWrite(TOUCH_RST, LOW);
    delay(100);
    digitalWrite(TOUCH_RST, HIGH);
    delay(100);

    uint8_t points = 0;
    Point ignored = {};
    bool ok = false;
    for (uint8_t i = 0; i < BEGIN_RETRY_COUNT; ++i) {
        ok = read_touch_raw(points, ignored);
        if (ok) break;
        delay(BEGIN_RETRY_DELAY_MS);
    }

    g_ready = ok;
    Serial.printf("[touch] FT6336 %s after boot probe, poll=%ums, boot points=%u\n",
                  ok ? "ready" : "not responding yet", TOUCH_POLL_MS, points);
    return ok;
}

bool get_touch(Point& point) {
    uint8_t count = 0;
    return read_touch_raw(count, point) && count > 0;
}

bool consume_cartoon_tap(Point& point) {
    uint32_t now = millis();
    bool due_poll = g_last_poll_ms == 0 || now - g_last_poll_ms >= TOUCH_POLL_MS;
    if (!due_poll && !g_was_touched) return false;
    if (due_poll) g_last_poll_ms = now;

    uint8_t count = 0;
    Point p = {};
    bool read_ok = read_touch_raw(count, p);
    if (!read_ok) {
        if (g_ready) Serial.println("[touch] read failed; retrying in loop");
        g_ready = false;
        g_was_touched = false;
        return false;
    }
    if (!g_ready) {
        g_ready = true;
        Serial.println("[touch] FT6336 recovered");
    }

    if (count == 0) {
        g_was_touched = false;
        return false;
    }

    if (g_was_touched) return false;
    g_was_touched = true;

    bool in_cartoon = point_in_cartoon(p);
    Serial.printf("[touch] tap points=%u x=%u y=%u in_cartoon=%d\n",
                  count, p.x, p.y, in_cartoon);

    if (g_last_tap_ms != 0 && now - g_last_tap_ms < TAP_DEBOUNCE_MS) return false;
    if (!in_cartoon) return false;

    point = p;
    g_last_tap_ms = now;
    return true;
}

}  // namespace touch_ft6336
