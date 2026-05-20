#pragma once
#include <Arduino.h>
#include <time.h>

// Waveshare 1.54-Touch-ePaper V2 板上的 PCF85063 RTC 芯片
// I2C 地址 0x51,板子上 SDA=GPIO47, SCL=GPIO48
//
// 关键功能:
//   - 自己有振荡器,断电后靠主电池(锂电池)继续走时,
//     主电池也掉电的话 OS 位会置 1,提示"时间已丢"
//   - 我们额外用 RAM_byte (0x03) 写 0x42 作为"已被我们设置过有效时间"的标记
class PCF85063 {
public:
    static constexpr uint8_t I2C_ADDR = 0x51;
    static constexpr uint8_t RAM_MAGIC = 0x42;     // "我们设过有效时间" 的标记值

    // 初始化 I2C 总线 (sda/scl 默认是 Waveshare V2 板子的引脚)
    // 重复调用安全
    bool begin(int sda_pin = 47, int scl_pin = 48, uint32_t freq_hz = 100000);

    // 检测 RTC 是否记录了"我们之前设过"的有效时间
    //   true  → 后续 readTime() 拿到的是真实历史时间
    //   false → 首次开机 / 电池没电过 / 没被我们写过,应该 fallback 到编译时间
    bool hasValidTime();

    // 读 RTC 当前时间到 struct tm。失败返回 false (I2C 错或时间无效)
    bool readTime(struct tm& out);

    // 把当前系统时间写入 RTC + 标 RAM_byte 表示有效
    bool writeTime(const struct tm& t);

private:
    bool readReg(uint8_t reg, uint8_t* buf, size_t n);
    bool writeReg(uint8_t reg, const uint8_t* buf, size_t n);
    bool writeReg(uint8_t reg, uint8_t val) { return writeReg(reg, &val, 1); }
};
