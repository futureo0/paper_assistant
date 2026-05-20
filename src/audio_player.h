#pragma once
#include <Arduino.h>

namespace audio_player {

bool begin();
bool is_playing();
void play_random_voice();
void shutdown();

}  // namespace audio_player
