#include "home_screen.h"
#include "cartoon.h"

#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <sys/time.h>

// ---- 布局常量 (200x200 屏幕) ----
constexpr int SCR_W = 200;
constexpr int SCR_H = 200;

// 卡通:150x150,水平居中,垂直从 y=44 开始 (顶部留约 44px 给时间)
constexpr int CARTOON_X = (SCR_W - cartoon_w) / 2;   // = 25
constexpr int CARTOON_Y = 44;

// 时间数字基线:y=80。FreeSansBold24pt7b 字符高度约 33px,baseline 落 80 意味着字符顶约在 47
// 这样 "14:05" 的下半部分 (y≈55-80) 会被卡通顶部 (y=44+) 局部覆盖,模拟 stack 层叠
constexpr int TIME_BASELINE_Y = 80;

// 日期文字基线:屏幕最底 y=195
constexpr int DATE_BASELINE_Y = 195;

template <typename Display>
static void draw_full(Display& display, const struct tm& now) {
    // 1. 白底
    display.fillScreen(GxEPD_WHITE);

    // 2. 时间数字 (背景层,大字号)
    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);

    display.setFont(&FreeSansBold24pt7b);
    display.setTextColor(GxEPD_BLACK);

    // 用 getTextBounds 测算文字宽度,实现水平居中
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(hhmm, 0, TIME_BASELINE_Y, &tx, &ty, &tw, &th);
    int time_x = (SCR_W - (int)tw) / 2 - tx;
    display.setCursor(time_x, TIME_BASELINE_Y);
    display.print(hhmm);

    // 3. 卡通图:覆盖在时间数字之上,bit=1 的像素画黑色,bit=0 不画 (透明)
    display.drawBitmap(CARTOON_X, CARTOON_Y, cartoon_bitmap,
                       cartoon_w, cartoon_h, GxEPD_BLACK);

    // 4. 底部日期条
    char date_str[24];
    static const char* wday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    snprintf(date_str, sizeof(date_str), "%d/%02d/%02d %s",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, wday[now.tm_wday]);
    display.setFont(&FreeSans9pt7b);
    int16_t dx, dy;
    uint16_t dw, dh;
    display.getTextBounds(date_str, 0, DATE_BASELINE_Y, &dx, &dy, &dw, &dh);
    int date_x = (SCR_W - (int)dw) / 2 - dx;
    display.setCursor(date_x, DATE_BASELINE_Y);
    display.print(date_str);
}

template <typename Display>
void render_home(Display& display, const struct tm& now, bool full_redraw) {
    if (full_redraw) {
        display.setFullWindow();
        display.firstPage();
        do {
            draw_full(display, now);
        } while (display.nextPage());
    } else {
        // 局部刷新:Task #6 实现。先复用全刷。
        display.setFullWindow();
        display.firstPage();
        do {
            draw_full(display, now);
        } while (display.nextPage());
    }
}

struct tm get_local_now() {
    time_t t = time(nullptr);
    struct tm out;
    localtime_r(&t, &out);
    return out;
}

// ---- 显式实例化,告诉编译器我们用的具体 Display 类型 ----
// (template 在 .cpp 里定义,需要显式实例化才能在别的 TU 链接)
#include <GxEPD2_BW.h>
template void render_home<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const struct tm&, bool);
