// ESP32-S3-Touch-ePaper-1.54 V2 — 首页 v1:卡通 + 实时时钟
// 这是草稿。等 Hello World 跑通后,把这个文件复制到 src/main.cpp 替换。
//
// 同时需要把 cartoon.h / home_screen.h / home_screen.cpp 从 drafts/home_v1/
// 复制或软链到 src/ (PlatformIO 默认只编译 src/ 下的 .cpp)。

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <sys/time.h>

#include "home_screen.h"

// ---- 引脚 ----
constexpr int8_t EPD_CS   = 11;
constexpr int8_t EPD_DC   = 10;
constexpr int8_t EPD_RST  = 9;
constexpr int8_t EPD_BUSY = 8;
constexpr int8_t EPD_SCK  = 12;
constexpr int8_t EPD_MOSI = 13;
constexpr int8_t EPD_PWR  = 6;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
  GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// 编译时刻注入初始时间:解析 __DATE__ "May 20 2026" + __TIME__ "15:31:00"
static void seed_clock_from_build_time() {
    struct tm tm_build = {};
    static const char* mon_names = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon_str[4] = {0};
    int day, year, hour, min, sec;
    sscanf(__DATE__, "%3s %d %d", mon_str, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
    const char* p = strstr(mon_names, mon_str);
    tm_build.tm_mon  = p ? (p - mon_names) / 3 : 0;
    tm_build.tm_mday = day;
    tm_build.tm_year = year - 1900;
    tm_build.tm_hour = hour;
    tm_build.tm_min  = min;
    tm_build.tm_sec  = sec;
    tm_build.tm_isdst = -1;
    time_t t = mktime(&tm_build);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n[boot] paper_assistant home v1");

    seed_clock_from_build_time();
    {
        char buf[32];
        struct tm now = get_local_now();
        strftime(buf, sizeof(buf), "%F %T", &now);
        Serial.printf("[clock] seeded with build time: %s\n", buf);
    }

    // EPD_PWR 是 active-low (P-MOSFET 高边开关): LOW=开,HIGH=关
    pinMode(EPD_PWR, OUTPUT);
    digitalWrite(EPD_PWR, LOW);
    delay(100);

    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
    display.init(115200, true, 2, false);
    display.setRotation(0);

    // 首次全刷
    struct tm now = get_local_now();
    render_home(display, now, /*full_redraw=*/true);
    display.hibernate();
    Serial.println("[done] initial render finished");
}

// 主循环:每秒检查一次分钟是否变化,变了就重绘
void loop() {
    static int last_minute = -1;
    struct tm now = get_local_now();
    if (now.tm_min != last_minute) {
        last_minute = now.tm_min;
        Serial.printf("[tick] %02d:%02d -> re-render\n", now.tm_hour, now.tm_min);
        // 注意:每次重绘前需要 wake up display
        digitalWrite(EPD_PWR, LOW);              // active-low: LOW 才是开
        delay(100);
        display.init(115200, false, 10, false);  // 第二个参数 false: 不打印日志
        render_home(display, now, /*full_redraw=*/true);  // TODO: 切局部刷新
        display.hibernate();
    }
    delay(1000);
}
