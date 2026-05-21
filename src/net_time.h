#pragma once
#include <Arduino.h>
#include "rtc_pcf85063.h"

// WiFi + NTP 校时模块
//
// 三层 fallback 设计的"最权威源":
//   1. (this) NTP    — 联网时拿到的精确时间
//   2. PCF85063 RTC  — 上次 NTP 成功后写入,断电由 backup 电池保持
//   3. 编译时间      — 上面两条都没救时的末日兜底
//
// 本模块只负责第 1 层,以及把成功结果"喂"给第 2 层 (RTC writeTime)。
namespace net_time {

constexpr const char* TZ_ASIA_SHANGHAI = "CST-8";  // POSIX TZ: UTC+8, no DST

using StatusCallback = void (*)(const char* msg);

struct SyncResult {
    bool wifi_ok;          // WiFi 连上了 (含可能走了 portal 配置)
    bool ntp_ok;           // NTP 拉到时间并写回 RTC 了
    bool entered_portal;   // 是否走过 AP portal (首次配 / 半夜 NVS 凭据连不上时为 true)
    time_t epoch_before;   // 同步前系统时间
    time_t epoch_after;    // 同步后系统时间 (ntp_ok=false 时为 0)
};

// 完整阻塞式流程:开 WiFi (或开 AP 配 portal) → NTP → 写 RTC → 关 WiFi
//
// wifi_connect_timeout_s: 已有凭据时连接超时,推荐 15 秒
// portal_timeout_s:       首次没凭据时 AP 配置等待超时,推荐 90 秒
//                         传 0 表示"禁止开 portal" (用于无人值守的 daily resync)
// status_cb:              进入 AP portal 模式时调一次,
//                         传 "Setup WiFi: PaperAssist-AP",
//                         调用方在屏幕画提示
SyncResult try_sync(PCF85063& rtc,
                    uint16_t wifi_connect_timeout_s = 15,
                    uint16_t portal_timeout_s = 90,
                    StatusCallback status_cb = nullptr);


// AP 模式时的 SSID,供调用方在屏幕/日志里引用,不要硬编码
constexpr const char* AP_SSID = "PaperAssist-AP";

}  // namespace net_time
