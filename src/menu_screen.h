#pragma once
#include <Arduino.h>
#include <GxEPD2_BW.h>

namespace menu_screen {

enum class Tile : uint8_t {
    None = 0,
    Wifi,
    Interaction,
    Volume,
    Battery,
};

template <typename Display>
void render_menu_full(Display& display, bool interaction_enabled, uint8_t volume_level, uint8_t volume_max,
                      uint8_t battery_percent, uint16_t battery_mv);

template <typename Display>
void render_interaction_tile(Display& display, bool interaction_enabled);

template <typename Display>
void render_volume_page(Display& display, uint8_t volume_level, uint8_t volume_max);

template <typename Display>
void render_volume_value(Display& display, uint8_t volume_level, uint8_t volume_max);

template <typename Display>
void render_wifi_page(Display& display, const char* msg);

template <typename Display>
void render_wifi_status(Display& display, const char* msg);

Tile tile_at(uint16_t x, uint16_t y);

}  // namespace menu_screen
