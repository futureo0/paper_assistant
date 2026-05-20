#pragma once
#include <Arduino.h>
#include <time.h>
#include <GxEPD2_BW.h>

// 屏幕主入口:把当前时间渲染成 "时间在后、卡通在前" 的 stack 布局
//
// 渲染顺序(从下到上 z-order):
//   1. 白底
//   2. 时间数字 (HH:MM,大字号,顶端起绘,占屏幕上 2/3 高度)
//   3. 卡通图位图 (150x150,水平居中,垂直靠下;顶部会盖住时间的下半部分)
//   4. 屏幕底部一个小条显示日期 (可选)
//
// full_redraw=true 时调用 firstPage/nextPage 全屏刷新 (慢但无残影)
// full_redraw=false 时只刷新时间区域 (局部刷新,留给 Task #6 实现)
template <typename Display>
void render_home(Display& display, const struct tm& now, bool full_redraw);

// 取当前时间的便捷函数:基于 setup 时设置的系统时间
struct tm get_local_now();
