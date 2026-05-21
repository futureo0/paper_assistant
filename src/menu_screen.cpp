#include "menu_screen.h"

namespace menu_screen {

constexpr int SCR_W = 200;
constexpr int SCR_H = 200;
constexpr int GAP = 8;
constexpr int TILE = 84;
constexpr int LEFT = 12;
constexpr int TOP = 12;
constexpr int RIGHT = LEFT + TILE + GAP;
constexpr int BOTTOM = TOP + TILE + GAP;
constexpr int LABEL_BASELINE_OFFSET = 72;
constexpr int VOLUME_DYNAMIC_Y = 56;
constexpr int VOLUME_DYNAMIC_H = 84;
constexpr int VOLUME_BAR_X = 25;
constexpr int VOLUME_BAR_Y = 118;
constexpr int VOLUME_BAR_W = 150;
constexpr int VOLUME_BAR_H = 16;
constexpr int WIFI_STATUS_Y = 136;
constexpr int WIFI_STATUS_H = 22;

template <typename Display>
static void draw_center_text(Display& display, const char* text, int x, int y, int w, int baseline) {
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(text, 0, baseline, &tx, &ty, &tw, &th);
    display.setCursor(x + (w - (int)tw) / 2 - tx, y + baseline);
    display.print(text);
}

template <typename Display>
static void draw_wifi_icon(Display& display, int cx, int cy) {
    int dot_y = cy + 24;
    display.fillCircle(cx, dot_y, 3, GxEPD_BLACK);
    display.drawLine(cx - 13, dot_y - 12, cx - 7, dot_y - 18, GxEPD_BLACK);
    display.drawLine(cx - 7, dot_y - 18, cx, dot_y - 20, GxEPD_BLACK);
    display.drawLine(cx, dot_y - 20, cx + 7, dot_y - 18, GxEPD_BLACK);
    display.drawLine(cx + 7, dot_y - 18, cx + 13, dot_y - 12, GxEPD_BLACK);
    display.drawLine(cx - 23, dot_y - 23, cx - 12, dot_y - 32, GxEPD_BLACK);
    display.drawLine(cx - 12, dot_y - 32, cx, dot_y - 35, GxEPD_BLACK);
    display.drawLine(cx, dot_y - 35, cx + 12, dot_y - 32, GxEPD_BLACK);
    display.drawLine(cx + 12, dot_y - 32, cx + 23, dot_y - 23, GxEPD_BLACK);
}

template <typename Display>
static void draw_touch_icon(Display& display, int cx, int cy, bool enabled) {
    display.drawRoundRect(cx - 14, cy - 18, 28, 38, 4, GxEPD_BLACK);
    display.fillCircle(cx, cy + 28, 3, GxEPD_BLACK);
    if (enabled) {
        display.drawCircle(cx + 15, cy - 11, 4, GxEPD_BLACK);
        display.drawCircle(cx + 21, cy - 17, 7, GxEPD_BLACK);
    } else {
        display.drawLine(cx + 17, cy - 22, cx + 29, cy - 10, GxEPD_BLACK);
        display.drawLine(cx + 29, cy - 22, cx + 17, cy - 10, GxEPD_BLACK);
    }
}

template <typename Display>
static void draw_volume_icon(Display& display, int cx, int cy, uint8_t level, uint8_t max_level) {
    display.fillRect(cx - 25, cy - 6, 10, 16, GxEPD_BLACK);
    display.fillTriangle(cx - 15, cy - 6, cx - 4, cy - 16, cx - 4, cy + 20, GxEPD_BLACK);
    display.fillTriangle(cx - 15, cy - 6, cx - 4, cy + 20, cx - 15, cy + 10, GxEPD_BLACK);
    if (level > 0) display.drawCircle(cx + 3, cy + 2, 10, GxEPD_BLACK);
    if (level > max_level / 2) display.drawCircle(cx + 3, cy + 2, 18, GxEPD_BLACK);
}

template <typename Display>
static void draw_empty_icon(Display& display, int cx, int cy) {
    display.drawFastHLine(cx - 16, cy + 2, 32, GxEPD_BLACK);
}

template <typename Display>
static void draw_tile(Display& display, int x, int y, const char* label) {
    display.drawRoundRect(x, y, TILE, TILE, 6, GxEPD_BLACK);
    display.drawRoundRect(x + 1, y + 1, TILE - 2, TILE - 2, 5, GxEPD_BLACK);
    display.setFont();
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    draw_center_text(display, label, x, y, TILE, LABEL_BASELINE_OFFSET);
}

template <typename Display>
static void draw_interaction_tile_content(Display& display, bool interaction_enabled) {
    display.fillRect(RIGHT + 4, TOP + 4, TILE - 8, TILE - 8, GxEPD_WHITE);
    draw_touch_icon(display, RIGHT + TILE / 2, TOP + 30, interaction_enabled);
    display.setFont();
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    draw_center_text(display, interaction_enabled ? "Touch ON" : "Touch OFF", RIGHT, TOP, TILE, LABEL_BASELINE_OFFSET);
}

template <typename Display>
static void draw_volume_value(Display& display, uint8_t volume_level, uint8_t volume_max) {
    display.fillRect(0, VOLUME_DYNAMIC_Y, SCR_W, VOLUME_DYNAMIC_H, GxEPD_WHITE);
    display.setFont();
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(3);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u/%u", volume_level, volume_max);
    draw_center_text(display, buf, 0, 0, SCR_W, 92);

    display.drawRect(VOLUME_BAR_X, VOLUME_BAR_Y, VOLUME_BAR_W, VOLUME_BAR_H, GxEPD_BLACK);
    int fill_w = 0;
    if (volume_max > 0) {
        fill_w = (VOLUME_BAR_W - 4) * volume_level / volume_max;
    }
    display.fillRect(VOLUME_BAR_X + 2, VOLUME_BAR_Y + 2, fill_w, VOLUME_BAR_H - 4, GxEPD_BLACK);
    display.setTextSize(1);
}

template <typename Display>
static void draw_wifi_status(Display& display, const char* msg) {
    display.fillRect(0, WIFI_STATUS_Y, SCR_W, WIFI_STATUS_H, GxEPD_WHITE);
    display.setFont();
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    draw_center_text(display, msg, 0, 0, SCR_W, 150);
}

template <typename Display>
void render_menu_full(Display& display, bool interaction_enabled, uint8_t volume_level, uint8_t volume_max) {
    display.setPartialWindow(0, 0, SCR_W, SCR_H);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        draw_tile(display, LEFT, TOP, "WiFi");
        draw_wifi_icon(display, LEFT + TILE / 2, TOP + 26);

        draw_tile(display, RIGHT, TOP, "");
        draw_interaction_tile_content(display, interaction_enabled);

        draw_tile(display, LEFT, BOTTOM, "Volume");
        draw_volume_icon(display, LEFT + TILE / 2, BOTTOM + 28, volume_level, volume_max);

        draw_tile(display, RIGHT, BOTTOM, "");
        draw_empty_icon(display, RIGHT + TILE / 2, BOTTOM + 31);
    } while (display.nextPage());
}

template <typename Display>
void render_interaction_tile(Display& display, bool interaction_enabled) {
    display.setPartialWindow(RIGHT, TOP, TILE, TILE);
    display.firstPage();
    do {
        display.fillRect(RIGHT, TOP, TILE, TILE, GxEPD_WHITE);
        draw_tile(display, RIGHT, TOP, "");
        draw_interaction_tile_content(display, interaction_enabled);
    } while (display.nextPage());
}

template <typename Display>
void render_volume_page(Display& display, uint8_t volume_level, uint8_t volume_max) {
    display.setPartialWindow(0, 0, SCR_W, SCR_H);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont();
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(2);
        draw_center_text(display, "Volume", 0, 0, SCR_W, 28);
        draw_volume_value(display, volume_level, volume_max);

        display.setTextSize(1);
        display.setCursor(18, 174);
        display.print("- tap left");
        display.setCursor(121, 174);
        display.print("tap right +");
        display.setCursor(55, 190);
        display.print("center: test");
    } while (display.nextPage());
}

template <typename Display>
void render_volume_value(Display& display, uint8_t volume_level, uint8_t volume_max) {
    display.setPartialWindow(0, VOLUME_DYNAMIC_Y, SCR_W, VOLUME_DYNAMIC_H);
    display.firstPage();
    do {
        draw_volume_value(display, volume_level, volume_max);
    } while (display.nextPage());
}

template <typename Display>
void render_wifi_page(Display& display, const char* msg) {
    display.setPartialWindow(0, 0, SCR_W, SCR_H);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont();
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(2);
        draw_center_text(display, "WiFi", 0, 0, SCR_W, 32);
        draw_wifi_icon(display, SCR_W / 2, 76);
        draw_wifi_status(display, msg);
        draw_center_text(display, "SSID: PaperAssist-AP", 0, 0, SCR_W, 170);
        draw_center_text(display, "BOOT: back", 0, 0, SCR_W, 190);
    } while (display.nextPage());
}

template <typename Display>
void render_wifi_status(Display& display, const char* msg) {
    display.setPartialWindow(0, WIFI_STATUS_Y, SCR_W, WIFI_STATUS_H);
    display.firstPage();
    do {
        draw_wifi_status(display, msg);
    } while (display.nextPage());
}

Tile tile_at(uint16_t x, uint16_t y) {
    if (x >= LEFT && x < LEFT + TILE && y >= TOP && y < TOP + TILE) return Tile::Wifi;
    if (x >= RIGHT && x < RIGHT + TILE && y >= TOP && y < TOP + TILE) return Tile::Interaction;
    if (x >= LEFT && x < LEFT + TILE && y >= BOTTOM && y < BOTTOM + TILE) return Tile::Volume;
    if (x >= RIGHT && x < RIGHT + TILE && y >= BOTTOM && y < BOTTOM + TILE) return Tile::Empty;
    return Tile::None;
}

}  // namespace menu_screen

template void menu_screen::render_menu_full<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, bool, uint8_t, uint8_t);
template void menu_screen::render_interaction_tile<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, bool);
template void menu_screen::render_volume_page<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, uint8_t, uint8_t);
template void menu_screen::render_volume_value<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, uint8_t, uint8_t);
template void menu_screen::render_wifi_page<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const char*);
template void menu_screen::render_wifi_status<GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>>(
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>&, const char*);
