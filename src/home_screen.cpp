#include "home_screen.h"
#include "cartoon.h"
#include "cartoon_mask.h"

#include <Fonts/FreeSansBold24pt7b.h>
#include <sys/time.h>

// ---- 布局常量 (200x200 屏幕) ----
constexpr int SCR_W = 200;
constexpr int SCR_H = 200;

// 卡通:180x180,水平居中,顶部留 15px 让时间数字露头
constexpr int CARTOON_X = (SCR_W - cartoon_w) / 2;   // = 10
constexpr int CARTOON_Y = 15;

// 时间数字 baseline:y=50。FreeSansBold24pt7b 字符高度 ~35px,
// 数字顶约 y=20,下半被卡通(y=15+)挡住,只露上方一条
constexpr int TIME_BASELINE_Y = 50;

// partial refresh 窄带:覆盖时间数字 + 卡通脑袋顶部
// 缩小到 60px 高 (上次是 80),减小 partial 模式下的"灰雾"感知面积
constexpr int PARTIAL_TOP = 0;
constexpr int PARTIAL_BOTTOM = 60;

template <typename Display>
static void draw_time(Display& display, const struct tm& now) {
    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
    display.setFont(&FreeSansBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);

    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(hhmm, 0, TIME_BASELINE_Y, &tx, &ty, &tw, &th);
    int time_x = (SCR_W - (int)tw) / 2 - tx;
    display.setCursor(time_x, TIME_BASELINE_Y);
    display.print(hhmm);
}

// 真正的"卡通在顶层":分两步
//   1. 用 mask 把卡通主体"形状"(不是矩形!)区域擦白
//      → mask bit=1 处画 WHITE,bit=0 处透明 (保留下层时间数字)
//   2. 用 cartoon 把卡通的黑像素画上去
//      → cartoon bit=1 处画 BLACK,bit=0 处透明 (主体内的白已经被 mask 擦好)
template <typename Display>
static void draw_cartoon(Display& display) {
    display.setFont();
    display.setTextSize(1);
    display.drawBitmap(CARTOON_X, CARTOON_Y, cartoon_mask_bitmap,
                       cartoon_mask_w, cartoon_mask_h, GxEPD_WHITE);
    display.drawBitmap(CARTOON_X, CARTOON_Y, cartoon_bitmap,
                       cartoon_w, cartoon_h, GxEPD_BLACK);
}

template <typename Display>
static void draw_full_content(Display& display, const struct tm& now) {
    display.fillScreen(GxEPD_WHITE);
    draw_time(display, now);          // 先画时间数字 (z=0)
    draw_cartoon(display);            // 卡通覆盖中下大部分 (z=1)
}

template <typename Display>
void render_home_full(Display& display, const struct tm& now) {
    display.setFullWindow();
    display.firstPage();
    do {
        draw_full_content(display, now);
    } while (display.nextPage());
}

template <typename Display>
void render_home_fast(Display& display, const struct tm& now) {
    display.setPartialWindow(0, 0, SCR_W, SCR_H);
    display.firstPage();
    do {
        draw_full_content(display, now);
    } while (display.nextPage());
}

template <typename Display>
void render_home_soft_clean(Display& display, const struct tm& now) {
    display.setPartialWindow(0, 0, SCR_W, SCR_H);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());

    render_home_fast(display, now);
}

// setup 阶段配 WiFi 时用:在底部 22px 白条上居中画一行 5x7 字
// 会盖住卡通脚部,但配网只是临时态,看完就走
template <typename Display>
static void draw_setup_msg(Display& display, const char* msg) {
    constexpr int MSG_BAND_Y = 178;
    constexpr int MSG_BAND_H = 22;
    constexpr int MSG_BASELINE = 192;
    display.fillRect(0, MSG_BAND_Y, SCR_W, MSG_BAND_H, GxEPD_WHITE);
    display.setFont();                     // 切回默认 5x7
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(msg, 0, MSG_BASELINE, &tx, &ty, &tw, &th);
    int x = (SCR_W - (int)tw) / 2 - tx;
    display.setCursor(x, MSG_BASELINE);
    display.print(msg);
}

template <typename Display>
void render_home_with_setup_msg(Display& display, const struct tm& now, const char* msg) {
    display.setFullWindow();
    display.firstPage();
    do {
        draw_full_content(display, now);
        draw_setup_msg(display, msg);
    } while (display.nextPage());
}

template <typename Display>
static void draw_powered_off_badge(Display& display) {
    constexpr int CX = SCR_W - 15;
    constexpr int CY = 13;
    constexpr int R = 7;

    display.fillRect(CX - 10, CY - 10, 20, 20, GxEPD_WHITE);
    // Power icon: open circle + vertical stroke. Kept icon-only to avoid text overflow.
    display.drawCircle(CX, CY, R, GxEPD_BLACK);
    display.drawCircle(CX, CY, R - 1, GxEPD_BLACK);
    display.fillRect(CX - 3, CY - R - 1, 7, 6, GxEPD_WHITE);
    display.drawFastVLine(CX, CY - R - 4, 9, GxEPD_BLACK);
    display.drawFastVLine(CX - 1, CY - R - 3, 7, GxEPD_BLACK);
}

template <typename Display>
void render_home_powered_off(Display& display, const struct tm& now) {
    display.setFullWindow();
    display.firstPage();
    do {
        draw_full_content(display, now);
        draw_powered_off_badge(display);
    } while (display.nextPage());
}

template <typename Display>
void render_home_partial_time(Display& display, const struct tm& now) {
    // 只擦写 partial window 这一带,屏幕其余像素物理保持
    display.setPartialWindow(0, PARTIAL_TOP, SCR_W, PARTIAL_BOTTOM - PARTIAL_TOP);
    display.firstPage();
    do {
        // GxEPD2 把所有绘制 clip 到 partial window 内
        display.fillScreen(GxEPD_WHITE);
        draw_time(display, now);
        draw_cartoon(display);        // 卡通 y>=80 的部分 GxEPD2 自动 clip 掉
    } while (display.nextPage());
}

struct tm get_local_now() {
    time_t t = time(nullptr);
    struct tm out;
    localtime_r(&t, &out);
    return out;
}

// ---- 显式实例化 ----
template void render_home_full<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&);
template void render_home_fast<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&);
template void render_home_soft_clean<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&);
template void render_home_partial_time<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&);
template void render_home_with_setup_msg<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&, const char*);
template void render_home_powered_off<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&);
