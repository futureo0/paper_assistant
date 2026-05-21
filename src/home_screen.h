#pragma once
#include <Arduino.h>
#include <time.h>
#include <GxEPD2_BW.h>

// 全屏重绘:擦除所有像素重新画 (擦鬼影、但会闪烁 1-2 秒)
// 开机第一次 + 每隔 N 次 partial 后定期清鬼影时调用
template <typename Display>
void render_home_full(Display& display, const struct tm& now);

// 页面返回首页时用:全屏内容走 SSD1681 differential partial update,
// 避免交互路径里出现黑白全屏震荡。
template <typename Display>
void render_home_fast(Display& display, const struct tm& now);

// 折中清屏:先用全屏 partial 刷一帧纯白,再用全屏 partial 画主页。
// 实测:肉眼上和直接 fast refresh 接近,保留为当前返回首页路径。
template <typename Display>
void render_home_soft_clean(Display& display, const struct tm& now);

// 只刷时间所在的横向窄带 (y=0..80) — 屏幕下方卡通主体物理保持
// 用 SSD1681 的 partial refresh,无闪烁、毫秒级完成
template <typename Display>
void render_home_partial_time(Display& display, const struct tm& now);

// 在常规首屏(时间+卡通)基础上,在底部画一行小字提示
// 仅用于 setup 阶段 WiFi 配 portal 时:render_home_with_setup_msg(d, now, "Setup WiFi: ...")
// 走 full refresh,字号 5x7 默认字体
template <typename Display>
void render_home_with_setup_msg(Display& display, const struct tm& now, const char* msg);

// 关机前最后一帧:保留当前表盘,右上角画 OFF 指示
template <typename Display>
void render_home_powered_off(Display& display, const struct tm& now);

// 取当前系统时间
struct tm get_local_now();
