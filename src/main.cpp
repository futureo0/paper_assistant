// ESP32-S3-Touch-ePaper-1.54 V2 — 首页 v2
// 卡通 180x180 + 24pt 时间数字 + partial refresh (每分钟无闪烁)

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "home_screen.h"
#include "net_time.h"
#include "rtc_pcf85063.h"

PCF85063 rtc;

// ---- 引脚 (Waveshare ESP32-S3-Touch-ePaper-1.54 V2) ----
constexpr int8_t EPD_CS   = 11;
constexpr int8_t EPD_DC   = 10;
constexpr int8_t EPD_RST  = 9;
constexpr int8_t EPD_BUSY = 8;
constexpr int8_t EPD_SCK  = 12;
constexpr int8_t EPD_MOSI = 13;
constexpr int8_t EPD_PWR     = 6;   // active-low: LOW=开,HIGH=关
constexpr int8_t BAT_CONTROL = 17;  // HIGH=保持锂电池供电锁存
constexpr int8_t BAT_KEY     = 18;  // PWR 按键输入,按下为 LOW

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
  GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

constexpr int DAILY_NTP_SYNC_HOUR = 3;
constexpr uint32_t SHUTDOWN_HOLD_MS = 2000;

static int g_last_ntp_attempt_day_key = -1;

static void hold_battery_power() {
    pinMode(BAT_CONTROL, OUTPUT);
    digitalWrite(BAT_CONTROL, HIGH);
}

static void release_battery_power() {
    digitalWrite(BAT_CONTROL, LOW);
}

static void configure_power_button() {
    pinMode(BAT_KEY, INPUT_PULLUP);
}

static bool power_button_pressed() {
    return digitalRead(BAT_KEY) == LOW;
}

static void configure_timezone() {
    setenv("TZ", net_time::TZ_ASIA_SHANGHAI, 1);
    tzset();
    Serial.printf("[clock] timezone = Asia/Shanghai (%s)\n", net_time::TZ_ASIA_SHANGHAI);
}

static int day_key(const struct tm& t) {
    return t.tm_year * 366 + t.tm_yday;
}

static void remember_current_ntp_attempt_day() {
    struct tm now = get_local_now();
    g_last_ntp_attempt_day_key = day_key(now);
}

static void draw_wifi_setup_status(const char* msg) {
    Serial.printf("[draw] setup status: %s\n", msg);
    struct tm now = get_local_now();
    render_home_with_setup_msg(display, now, msg);
}

static bool visible_minute_changed(const net_time::SyncResult& result) {
    if (!result.ntp_ok) return false;

    struct tm before = {};
    struct tm after = {};
    localtime_r(&result.epoch_before, &before);
    localtime_r(&result.epoch_after, &after);

    return before.tm_hour != after.tm_hour || before.tm_min != after.tm_min;
}

static void shutdown_now() {
    Serial.println("[power] shutdown requested");
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);

    struct tm now = get_local_now();
    Serial.println("[draw] powered-off final frame");
    render_home_powered_off(display, now);

    digitalWrite(EPD_PWR, HIGH);   // active-low: cut e-paper panel power
    delay(100);
    Serial.println("[power] releasing battery hold");
    release_battery_power();

    // USB 插着时 BAT_Control 不能真正断整板电源,用 deep sleep 避免继续跑 loop。
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    Serial.flush();
    esp_deep_sleep_start();
}

static bool shutdown_hold_detected() {
    static uint32_t pressed_since = 0;
    if (!power_button_pressed()) {
        pressed_since = 0;
        return false;
    }

    if (pressed_since == 0) {
        pressed_since = millis();
        return false;
    }

    return millis() - pressed_since >= SHUTDOWN_HOLD_MS;
}

// 编译时刻注入初始时间,断电会丢,下个 task 接 PCF85063 解决
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

// 鬼影累积控制:每隔 N 分钟做一次 full refresh 把屏幕"洗"干净
constexpr int FULL_REFRESH_EVERY_N_MIN = 30;

// 同步 RTC 时间到 ESP32 系统时钟
static void sync_system_clock_from(const struct tm& t) {
    struct tm copy = t;                          // mktime 会修改入参
    time_t epoch = mktime(&copy);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
}

void setup() {
    hold_battery_power();
    configure_power_button();

    Serial.begin(115200);
    delay(2000);
    Serial.println("\n[boot] paper_assistant home v5 (battery hold + rtc + ntp + partial)");
    Serial.println("[power] battery hold enabled on GPIO17");
    Serial.println("[power] hold PWR for 2s to shut down");
    configure_timezone();

    // ===== 时间初始化 =====
    // 流程: RTC 通信 OK + 有效时间 → 用 RTC; 否则用编译时间 + 写回 RTC
    bool rtc_ok = rtc.begin();
    if (rtc_ok && rtc.hasValidTime()) {
        struct tm rtc_now;
        if (rtc.readTime(rtc_now)) {
            sync_system_clock_from(rtc_now);
            Serial.printf("[clock] from RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                          rtc_now.tm_year + 1900, rtc_now.tm_mon + 1, rtc_now.tm_mday,
                          rtc_now.tm_hour, rtc_now.tm_min, rtc_now.tm_sec);
        }
    } else {
        Serial.println("[clock] RTC invalid/missing → fallback to build time");
        seed_clock_from_build_time();
        if (rtc_ok) {
            // 把编译时间写回 RTC,后续断电再开机能保留
            struct tm now = get_local_now();
            rtc.writeTime(now);
        }
    }
    {
        char buf[32];
        struct tm now = get_local_now();
        strftime(buf, sizeof(buf), "%F %T", &now);
        Serial.printf("[clock] system time = %s\n", buf);
    }

    // ===== 屏幕初始化 =====
    pinMode(EPD_PWR, OUTPUT);
    digitalWrite(EPD_PWR, LOW);    // active-low: LOW 才开
    delay(100);

    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
    display.init(115200, true, 10, false);
    display.setRotation(0);

    Serial.printf("[geom] display.width()=%d, height()=%d, pages=%d\n",
                  display.width(), display.height(), display.pages());

    // 防鬼影震荡:全黑 → 全白 → 全白,把之前累积的灰像素彻底清掉
    // 一次性开销 ~4 秒,只在 setup 跑
    Serial.println("[wash] anti-ghost flush start");
    display.setFullWindow();
    for (int i = 0; i < 3; i++) {
        display.firstPage();
        do {
            display.fillScreen(i == 0 ? GxEPD_BLACK : GxEPD_WHITE);
        } while (display.nextPage());
    }
    Serial.println("[wash] anti-ghost flush done");

    struct tm now = get_local_now();
    Serial.println("[draw] initial full refresh");
    render_home_full(display, now);

    Serial.println("[ntp] startup sync begin");
    net_time::SyncResult startup_sync = net_time::try_sync(rtc, 15, 90, draw_wifi_setup_status);
    if (startup_sync.ntp_ok) {
        remember_current_ntp_attempt_day();
    }
    if (startup_sync.entered_portal || visible_minute_changed(startup_sync)) {
        now = get_local_now();
        Serial.println("[draw] full refresh after startup NTP");
        render_home_full(display, now);
    }

    Serial.println("[done] setup");
}

// loop:每分钟检测一次,变了就 partial refresh
// 每 FULL_REFRESH_EVERY_N_MIN 次 partial 后做一次 full 清鬼影
void loop() {
    static int last_minute = -1;
    static int partial_cnt = 0;
    if (shutdown_hold_detected()) {
        shutdown_now();
    }

    struct tm now = get_local_now();

    if (now.tm_hour == DAILY_NTP_SYNC_HOUR &&
        day_key(now) != g_last_ntp_attempt_day_key) {
        Serial.println("[ntp] daily sync begin");
        net_time::SyncResult daily_sync = net_time::try_sync(rtc, 15, 0, nullptr);
        now = get_local_now();
        remember_current_ntp_attempt_day();

        if (daily_sync.ntp_ok && visible_minute_changed(daily_sync)) {
            Serial.println("[draw] full refresh after daily NTP");
            render_home_full(display, now);
            last_minute = now.tm_min;
            partial_cnt = 0;
        }
    }

    if (now.tm_min != last_minute) {
        last_minute = now.tm_min;
        if (partial_cnt >= FULL_REFRESH_EVERY_N_MIN) {
            Serial.printf("[tick] %02d:%02d -> FULL (anti-ghost)\n",
                          now.tm_hour, now.tm_min);
            render_home_full(display, now);
            partial_cnt = 0;
        } else {
            Serial.printf("[tick] %02d:%02d -> partial #%d\n",
                          now.tm_hour, now.tm_min, partial_cnt + 1);
            render_home_partial_time(display, now);
            partial_cnt++;
        }
    }
    delay(1000);
}
