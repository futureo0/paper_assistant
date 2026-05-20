#include "rtc_pcf85063.h"
#include <Wire.h>

// BCD 编解码:RTC 寄存器存 BCD (每 4 bit 一位十进制)
static inline uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static inline uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

bool PCF85063::begin(int sda_pin, int scl_pin, uint32_t freq_hz) {
    Wire.begin(sda_pin, scl_pin, freq_hz);
    // 探测一下:写 0 字节看 ACK
    Wire.beginTransmission(I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("[rtc] PCF85063 not responding on I2C");
        return false;
    }
    Serial.println("[rtc] PCF85063 ACK ok");

    // 确保 Control_1 是正常运行 + 24h 模式
    // 默认值就是 0x00,我们写一次防止上电时是别的状态
    writeReg(0x00, (uint8_t)0x00);

    return true;
}

bool PCF85063::hasValidTime() {
    // 双重判定:
    //   1. RAM_byte (0x03) 是不是我们的 magic 值
    //   2. Seconds 寄存器的 OS (bit 7) == 0 (振荡器没掉过电)
    uint8_t ram = 0, sec = 0;
    if (!readReg(0x03, &ram, 1)) return false;
    if (!readReg(0x04, &sec, 1)) return false;
    bool ram_ok = (ram == RAM_MAGIC);
    bool osc_ok = ((sec & 0x80) == 0);
    Serial.printf("[rtc] ram=0x%02X (expect 0x%02X), OS=%d → %s\n",
                  ram, RAM_MAGIC, (sec >> 7) & 1, (ram_ok && osc_ok) ? "valid" : "invalid");
    return ram_ok && osc_ok;
}

bool PCF85063::readTime(struct tm& out) {
    uint8_t r[7];
    if (!readReg(0x04, r, 7)) return false;
    out.tm_sec  = bcd2dec(r[0] & 0x7F);
    out.tm_min  = bcd2dec(r[1] & 0x7F);
    out.tm_hour = bcd2dec(r[2] & 0x3F);
    out.tm_mday = bcd2dec(r[3] & 0x3F);
    out.tm_wday = r[4] & 0x07;
    out.tm_mon  = bcd2dec(r[5] & 0x1F) - 1;       // tm_mon 是 0-11
    out.tm_year = bcd2dec(r[6]) + 100;            // tm_year 是 1900 起;RTC 是 0-99 表示 2000-2099
    out.tm_isdst = -1;
    return true;
}

bool PCF85063::writeTime(const struct tm& t) {
    uint8_t r[7];
    r[0] = dec2bcd(t.tm_sec) & 0x7F;              // 写 0 到 OS bit,即同时清掉 "OS=1 振荡器停过" 标志
    r[1] = dec2bcd(t.tm_min) & 0x7F;
    r[2] = dec2bcd(t.tm_hour) & 0x3F;             // 24h 模式
    r[3] = dec2bcd(t.tm_mday) & 0x3F;
    r[4] = t.tm_wday & 0x07;
    r[5] = dec2bcd(t.tm_mon + 1) & 0x1F;
    r[6] = dec2bcd(t.tm_year - 100);              // 2026 - 1900 = 126,- 100 = 26
    if (!writeReg(0x04, r, 7)) return false;
    // 写 RAM_byte 标记
    if (!writeReg(0x03, RAM_MAGIC)) return false;
    Serial.printf("[rtc] wrote %04d-%02d-%02d %02d:%02d:%02d wday=%d\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec, t.tm_wday);
    return true;
}

bool PCF85063::readReg(uint8_t reg, uint8_t* buf, size_t n) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;     // false = no stop, restart 读
    size_t got = Wire.requestFrom((uint8_t)I2C_ADDR, (uint8_t)n);
    if (got != n) return false;
    for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

bool PCF85063::writeReg(uint8_t reg, const uint8_t* buf, size_t n) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    for (size_t i = 0; i < n; i++) Wire.write(buf[i]);
    return Wire.endTransmission() == 0;
}
