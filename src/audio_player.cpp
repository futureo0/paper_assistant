#include "audio_player.h"

#include "generated/voice_assets.h"

#include <Wire.h>
#include <driver/i2s.h>
#include <esp_err.h>

namespace audio_player {

static constexpr uint8_t ES8311_ADDR = 0x18;
static constexpr int AUDIO_PWR = 42;   // active-low
static constexpr int PA_CTRL = 46;
static constexpr int I2S_MCLK = 14;
static constexpr int I2S_BCLK = 15;
static constexpr int I2S_DOUT = 45;
static constexpr int I2S_LRCK = 38;
static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
static constexpr uint32_t SAMPLE_RATE = 48000;
static constexpr size_t WAV_HEADER_LEN = 44;
static constexpr size_t FRAMES_PER_CHUNK = 256;

struct Clip {
    const uint8_t* wav;
    size_t len;
};

static TaskHandle_t g_play_task = nullptr;
static bool g_ready = false;
static bool g_i2s_ready = false;

static bool write_reg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool read_reg(uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(ES8311_ADDR, (uint8_t)1) != 1) return false;
    value = Wire.read();
    return true;
}

static bool init_i2s() {
    if (g_i2s_ready) return true;

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = 6;
    cfg.dma_buf_len = FRAMES_PER_CHUNK;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = SAMPLE_RATE * 256;
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    cfg.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_MCLK;
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_LRCK;
    pins.data_out_num = I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[audio] i2s_driver_install failed: %d\n", err);
        return false;
    }
    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[audio] i2s_set_pin failed: %d\n", err);
        return false;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    g_i2s_ready = true;
    return true;
}

static bool init_es8311() {
    pinMode(AUDIO_PWR, OUTPUT);
    digitalWrite(AUDIO_PWR, LOW);
    pinMode(PA_CTRL, OUTPUT);
    digitalWrite(PA_CTRL, LOW);
    delay(20);

    uint8_t id = 0;
    if (!read_reg(0xFD, id)) {
        Serial.println("[audio] ES8311 not responding");
        return false;
    }
    Serial.printf("[audio] ES8311 id=0x%02X\n", id);

    bool ok = true;
    // Minimal ES8311 DAC init distilled from Espressif esp_codec_dev for 48k/16-bit I2S slave.
    ok &= write_reg(0x44, 0x08);
    ok &= write_reg(0x44, 0x08);
    ok &= write_reg(0x01, 0x30);
    ok &= write_reg(0x02, 0x00);
    ok &= write_reg(0x03, 0x10);
    ok &= write_reg(0x16, 0x24);
    ok &= write_reg(0x04, 0x10);
    ok &= write_reg(0x05, 0x00);
    ok &= write_reg(0x0B, 0x00);
    ok &= write_reg(0x0C, 0x00);
    ok &= write_reg(0x10, 0x1F);
    ok &= write_reg(0x11, 0x7F);
    ok &= write_reg(0x00, 0x80);
    ok &= write_reg(0x00, 0x80);  // slave mode
    ok &= write_reg(0x01, 0x3F);  // use external MCLK
    ok &= write_reg(0x06, 0x03);  // 48k coeff bclk div=4, non-inverted SCLK
    ok &= write_reg(0x13, 0x10);
    ok &= write_reg(0x1B, 0x0A);
    ok &= write_reg(0x1C, 0x6A);
    ok &= write_reg(0x44, 0x58);
    ok &= write_reg(0x09, 0x0C);  // DAC: I2S, 16-bit, unmuted
    ok &= write_reg(0x0A, 0x4C);  // ADC unused
    ok &= write_reg(0x02, 0x00);  // 12.288MHz / 48k coeff
    ok &= write_reg(0x05, 0x00);
    ok &= write_reg(0x03, 0x10);
    ok &= write_reg(0x04, 0x10);
    ok &= write_reg(0x07, 0x00);
    ok &= write_reg(0x08, 0xFF);
    ok &= write_reg(0x17, 0xBF);
    ok &= write_reg(0x0E, 0x02);
    ok &= write_reg(0x12, 0x00);
    ok &= write_reg(0x14, 0x1A);
    ok &= write_reg(0x0D, 0x01);
    ok &= write_reg(0x15, 0x40);
    ok &= write_reg(0x37, 0x08);
    ok &= write_reg(0x45, 0x00);
    ok &= write_reg(0x31, 0x00);  // DAC unmute
    ok &= write_reg(0x32, 0xD0);  // moderate volume

    digitalWrite(PA_CTRL, HIGH);
    if (!ok) Serial.println("[audio] ES8311 init write failed");
    return ok;
}

bool begin() {
    if (g_ready) return true;
    if (!init_i2s()) return false;
    if (!init_es8311()) return false;
    g_ready = true;
    Serial.println("[audio] ready");
    return true;
}

bool is_playing() {
    return g_play_task != nullptr;
}

static bool wav_payload(const Clip& clip, const int16_t*& samples, size_t& sample_count) {
    if (clip.len <= WAV_HEADER_LEN) return false;
    if (memcmp(clip.wav, "RIFF", 4) != 0 || memcmp(clip.wav + 8, "WAVE", 4) != 0) return false;
    samples = reinterpret_cast<const int16_t*>(clip.wav + WAV_HEADER_LEN);
    sample_count = (clip.len - WAV_HEADER_LEN) / sizeof(int16_t);
    return true;
}

static void play_task(void* arg) {
    Clip clip = *static_cast<Clip*>(arg);
    delete static_cast<Clip*>(arg);

    if (!begin()) {
        g_play_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    const int16_t* mono = nullptr;
    size_t mono_count = 0;
    if (!wav_payload(clip, mono, mono_count)) {
        Serial.println("[audio] invalid wav");
        g_play_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    int16_t stereo[FRAMES_PER_CHUNK * 2];
    size_t pos = 0;
    while (pos < mono_count) {
        size_t frames = min(FRAMES_PER_CHUNK, mono_count - pos);
        for (size_t i = 0; i < frames; ++i) {
            int16_t s = mono[pos + i];
            stereo[i * 2] = s;
            stereo[i * 2 + 1] = s;
        }
        size_t bytes_written = 0;
        i2s_write(I2S_PORT, stereo, frames * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
        pos += frames;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    g_play_task = nullptr;
    vTaskDelete(nullptr);
}

void play_random_voice() {
    if (is_playing()) return;
    Clip* clip = new Clip;
    if (esp_random() & 1) {
        *clip = {voice_what_wav, voice_what_wav_len};
        Serial.println("[audio] play: gan shen me");
    } else {
        *clip = {voice_leave_wav, voice_leave_wav_len};
        Serial.println("[audio] play: bu yao da rao lulu");
    }
    xTaskCreatePinnedToCore(play_task, "voice", 4096, clip, 2, &g_play_task, 1);
}

void shutdown() {
    if (g_i2s_ready) {
        i2s_zero_dma_buffer(I2S_PORT);
    }
    digitalWrite(PA_CTRL, LOW);
    digitalWrite(AUDIO_PWR, HIGH);
}

}  // namespace audio_player
