#pragma once
#include <Arduino.h>

namespace audio_player {

bool begin();
bool is_playing();
uint8_t volume_level();
uint8_t volume_max();
bool set_volume_level(uint8_t level);
void adjust_volume(int8_t delta);
void play_random_voice();
void shutdown();

}  // namespace audio_player
