#include "net_time.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

namespace net_time {

// 三级 NTP fallback:国内首选阿里云,补一个国内 pool,再补一个国际公共源
static constexpr const char* NTP1 = "ntp.aliyun.com";
static constexpr const char* NTP2 = "cn.pool.ntp.org";
static constexpr const char* NTP3 = "time.windows.com";

// AP 回调转发用 — WiFiManager.setAPCallback 不支持携带 user data,
// 用 file-static 指针在 try_sync 入口/出口配对设置
static StatusCallback g_status_cb = nullptr;
static bool g_entered_portal = false;

static void ap_mode_callback(WiFiManager* /*wm*/) {
    // 这个回调发生在 autoConnect 决定要开 AP portal 的瞬间
    g_entered_portal = true;
    if (g_status_cb) {
        static char msg[48];
        snprintf(msg, sizeof(msg), "Setup WiFi: %s", AP_SSID);
        g_status_cb(msg);
    }
    Serial.printf("[net] entered AP portal: SSID=%s, IP=192.168.4.1\n", AP_SSID);
}

static bool wait_ntp_sync(uint32_t timeout_ms) {
    // sntp_get_sync_status() 是 ESP-IDF 内置 SNTP 客户端的状态机
    // 比 getLocalTime() 可靠 — 后者只检查 "epoch 是否 > 2017",
    // 已经从 RTC 设过有效时间的情况下会瞬间返回 true,假阳性
    uint32_t deadline = millis() + timeout_ms;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        if ((int32_t)(millis() - deadline) >= 0) return false;
        delay(100);
    }
    return true;
}

static void shutdown_wifi() {
    WiFi.disconnect(true, false);   // true=断开, false=不清 NVS 凭据
    WiFi.mode(WIFI_OFF);
}

SyncResult try_sync(PCF85063& rtc,
                    uint16_t wifi_connect_timeout_s,
                    uint16_t portal_timeout_s,
                    StatusCallback status_cb) {
    SyncResult r = {};
    r.epoch_before = time(nullptr);

    g_status_cb = status_cb;
    g_entered_portal = false;

    WiFiManager wm;
    wm.setDebugOutput(false);                                 // 别污染串口
    wm.setConnectTimeout(wifi_connect_timeout_s);             // 连存量 WiFi 的超时
    wm.setConfigPortalTimeout(portal_timeout_s);              // AP portal 等待超时
    wm.setEnableConfigPortal(portal_timeout_s > 0);            // daily resync 不打扰用户配网
    wm.setAPCallback(ap_mode_callback);
    wm.setMinimumSignalQuality(8);                            // 太弱的 AP 直接跳过

    Serial.println("[net] WiFi: starting autoConnect");
    bool wifi_ok = wm.autoConnect(AP_SSID);
    g_status_cb = nullptr;
    r.entered_portal = g_entered_portal;
    g_entered_portal = false;

    if (!wifi_ok) {
        Serial.println("[net] WiFi: failed/timeout");
        shutdown_wifi();
        return r;
    }
    r.wifi_ok = true;
    Serial.printf("[net] WiFi: connected, IP=%s, RSSI=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());

    // 触发 SNTP:configTzTime 会保留 Asia/Shanghai 的本地时间语义
    Serial.printf("[ntp] querying %s, %s, %s\n", NTP1, NTP2, NTP3);
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime(TZ_ASIA_SHANGHAI, NTP1, NTP2, NTP3);

    if (!wait_ntp_sync(10000)) {
        Serial.println("[ntp] timeout (10s)");
        shutdown_wifi();
        return r;
    }

    r.epoch_after = time(nullptr);
    r.ntp_ok = true;

    // 喂给第二层持久化层
    struct tm now;
    localtime_r(&r.epoch_after, &now);
    rtc.writeTime(now);

    char buf[32];
    strftime(buf, sizeof(buf), "%F %T", &now);
    long delta = (long)(r.epoch_after - r.epoch_before);
    Serial.printf("[ntp] synced: %s (delta=%ld s vs prior)\n", buf, delta);

    shutdown_wifi();
    return r;
}

}  // namespace net_time
