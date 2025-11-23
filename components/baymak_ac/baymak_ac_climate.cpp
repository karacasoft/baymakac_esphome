#include "esphome/components/climate/climate_mode.h"
#include "esphome/core/log.h"
#include "esphome/components/remote_base/remote_base.h"
#include "baymak_ac_climate.hpp"
#include "protocol.hpp"
#include <string>
#include <cmath>

namespace esphome {
namespace baymak_ac_ns {

static const char *TAG = "baymak_ac_component.component";

// ---------- Small helpers for protocol encoding ----------

enum AcModeKind {
  AC_MODE_COOL,
  AC_MODE_HEAT,
  AC_MODE_DRY,
  AC_MODE_FAN_ONLY,
  AC_MODE_AUTO
};

static AcModeKind map_mode(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT:
      return AC_MODE_HEAT;
    case climate::CLIMATE_MODE_DRY:
      return AC_MODE_DRY;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return AC_MODE_FAN_ONLY;
    case climate::CLIMATE_MODE_AUTO:
      return AC_MODE_AUTO;
    case climate::CLIMATE_MODE_COOL:
    default:
      return AC_MODE_COOL;
  }
}

static float clamp_and_step_temp(float t) {
  if (std::isnan(t))
    t = 24.0f;
  if (t < 16.0f)
    t = 16.0f;
  if (t > 30.0f)
    t = 30.0f;
  // step 0.5
  t = std::round(t * 2.0f) / 2.0f;
  return t;
}

// vertical swing state s: 0..6 (0/6 = full swing, 1..5 = fixed)
// lr_on: left-right swing on/off
static uint32_t encode_W0(float temp_c, uint8_t vert_state, bool lr_on) {
  uint8_t full_deg = static_cast<uint8_t>(std::floor(temp_c));
  uint8_t base_temp = static_cast<uint8_t>(8 * full_deg - 57);  // base temp byte
  uint8_t temp_byte = static_cast<uint8_t>(base_temp + vert_state - 7);

  bool half = (temp_c - full_deg) >= 0.5f;

  uint8_t b0 = half ? 0x80 : 0x00;      // bit7 is half-degree flag
  uint8_t b1 = lr_on ? 0x00 : 0xE0;     // 0x00 = LR swing ON, 0xE0 = OFF
  uint8_t b2 = temp_byte;               // temp + vertical swing
  uint8_t b3 = 0xC3;                    // constant

  return (static_cast<uint32_t>(b0) << 24) |
         (static_cast<uint32_t>(b1) << 16) |
         (static_cast<uint32_t>(b2) << 8) |
         static_cast<uint32_t>(b3);
}

// W1: Mode, fan, health, sleep, turbo, silence
static uint32_t encode_W1(AcModeKind mode, climate::ClimateFanMode fan_mode,
                          bool health, bool sleep) {
  uint8_t b0 = 0x00;

  // Mode group in b1
  uint8_t b1;
  switch (mode) {
    case AC_MODE_HEAT:
      b1 = 0x80;
      break;
    case AC_MODE_DRY:
      b1 = 0x40;
      break;
    case AC_MODE_FAN_ONLY:
      b1 = 0xC0;
      break;
    case AC_MODE_AUTO:
      b1 = 0x00;
      break;
    case AC_MODE_COOL:
    default:
      b1 = 0x20;
      break;
  }

  if (health)
    b1 |= 0x10;  // health flag
  if (sleep)
    b1 |= 0x04;  // sleep flag

  uint8_t b2 = 0x00;
  uint8_t b3;

  // Map ESPHome fan modes → protocol
  switch (fan_mode) {
    case climate::CLIMATE_FAN_LOW:
      b3 = 0x60;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      b3 = 0x40;
      break;
    case climate::CLIMATE_FAN_HIGH:
      b3 = 0x20;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      b3 = 0xA0;
      break;
  }

  // (Turbo / Silence flags could be added later via b2 |= 0x40/0x80)

  return (static_cast<uint32_t>(b0) << 24) |
         (static_cast<uint32_t>(b1) << 16) |
         (static_cast<uint32_t>(b2) << 8) |
         static_cast<uint32_t>(b3);
}

// W2: mode class, health bit, timer bit, misc flags
static uint8_t encode_W2_b2(AcModeKind mode, bool health, bool timer_on) {
  uint8_t base = (mode == AC_MODE_HEAT) ? 0x30 : 0x20;

  if (health)
    base |= 0x02;

  if (timer_on && mode == AC_MODE_HEAT)
    base = 0x60;  // matches Heat+Timer rows

  return base;
}

// We keep W2_b0 simple but deterministic, based on mode.
// 0x02 is used for Heat/Cool "vane" states in the captures.
// 0x06 is used for Auto/Dry/Fan-only.
static uint8_t encode_W2_b0(AcModeKind mode) {
  switch (mode) {
    case AC_MODE_HEAT:
    case AC_MODE_COOL:
      return 0x02;
    case AC_MODE_AUTO:
    case AC_MODE_DRY:
    case AC_MODE_FAN_ONLY:
    default:
      return 0x06;
  }
}

static uint32_t encode_W2(AcModeKind mode, bool health, bool timer_on) {
  uint8_t b0 = encode_W2_b0(mode);
  uint8_t b1 = 0x00;
  uint8_t b2 = encode_W2_b2(mode, health, timer_on);
  uint8_t b3 = 0x00;

  return (static_cast<uint32_t>(b0) << 24) |
         (static_cast<uint32_t>(b1) << 16) |
         (static_cast<uint32_t>(b2) << 8) |
         static_cast<uint32_t>(b3);
}

// Checksum over W0..W2 (12 bytes)
static uint8_t compute_checksum(uint32_t w0, uint32_t w1, uint32_t w2) {
  uint8_t bytes[12] = {
      static_cast<uint8_t>(w0 >> 24), static_cast<uint8_t>(w0 >> 16),
      static_cast<uint8_t>(w0 >> 8),  static_cast<uint8_t>(w0),
      static_cast<uint8_t>(w1 >> 24), static_cast<uint8_t>(w1 >> 16),
      static_cast<uint8_t>(w1 >> 8),  static_cast<uint8_t>(w1),
      static_cast<uint8_t>(w2 >> 24), static_cast<uint8_t>(w2 >> 16),
      static_cast<uint8_t>(w2 >> 8),  static_cast<uint8_t>(w2),
  };

  uint32_t sum = 0;
  for (int i = 0; i < 12; i++)
    sum += bytes[i];

  return static_cast<uint8_t>(sum & 0xFF);
}

// Map ESPHome swing mode → vertical state / LR flag
// vert_state: 0..6 (0/6 full), 1..5 fixed positions
static void swing_to_vane(climate::ClimateSwingMode swing_mode,
                          uint8_t &vert_state, bool &lr_on) {
  switch (swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      vert_state = 0;  // full vertical swing variant A
      lr_on = true;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      vert_state = 0;  // full vertical swing variant A
      lr_on = false;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      vert_state = 3;  // middle
      lr_on = true;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      vert_state = 3;  // middle fixed
      lr_on = false;
      break;
  }
}

// ---------- Component methods ----------

void BaymakACComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  if (std::isnan(this->target_temperature)) {
    this->target_temperature = 30.0f;
  }
}

void BaymakACComponent::loop() {
}

void BaymakACComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Baymak AC Component");
}

climate::ClimateTraits BaymakACComponent::traits() {
  climate::ClimateTraits traits;

  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
      // climate::CLIMATE_MODE_AUTO,
  });

  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });

  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
      climate::CLIMATE_SWING_BOTH,
      climate::CLIMATE_SWING_HORIZONTAL,
  });

  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_temperature_step(0.5f);

  return traits;
}

void BaymakACComponent::control(const climate::ClimateCall &call) {
  // Apply requested changes to internal state
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }

  if (call.get_fan_mode().has_value()) {
    this->fan_mode = call.get_fan_mode();
  }

  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();
  }

  // Remember last non-OFF state so OFF can still send a deterministic frame
  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->last_mode_ = this->mode;
    this->last_fan_mode_ = this->fan_mode;
    this->last_swing_mode_ = this->swing_mode;
    this->last_target_temperature_ = this->target_temperature;
  }

  // Send IR frame for the new state
  this->send_frame_();

  // Publish the new state to Home Assistant
  this->publish_state();
}

void BaymakACComponent::send_frame_() {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured, cannot send IR frame");
    return;
  }

  bool power_off = (this->mode == climate::CLIMATE_MODE_OFF);

  // Use last known non-off state for encoding when turning OFF
  climate::ClimateMode eff_mode =
      power_off ? this->last_mode_ : this->mode;
  climate::ClimateFanMode eff_fan =
      power_off ? this->last_fan_mode_.value_or(climate::CLIMATE_FAN_AUTO) : this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO);
  climate::ClimateSwingMode eff_swing =
      power_off ? this->last_swing_mode_ : this->swing_mode;
  float eff_temp =
      power_off ? this->last_target_temperature_ : this->target_temperature;

  eff_temp = clamp_and_step_temp(eff_temp);

  AcModeKind ac_mode = map_mode(eff_mode);

  // For now we don't expose these from ESPHome controls; keep them false.
  bool health = false;
  bool sleep = false;
  bool timer_on = false;

  uint8_t vert_state = 3;
  bool lr_on = false;
  swing_to_vane(eff_swing, vert_state, lr_on);

  uint32_t w0 = encode_W0(eff_temp, vert_state, lr_on);
  uint32_t w1 = encode_W1(ac_mode, eff_fan, health, sleep);
  uint32_t w2;

  if (power_off) {
    // OFF frame: W0/W1 stay as "current" climate state, W2 pattern matches
    // the single OFF sample: b0=0x05, b1=0x00, b2=0x00, b3=0x00.
    uint8_t b0 = 0x05;
    uint8_t b1 = 0x00;
    uint8_t b2 = 0x00;
    uint8_t b3 = 0x00;
    w2 = (static_cast<uint32_t>(b0) << 24) |
         (static_cast<uint32_t>(b1) << 16) |
         (static_cast<uint32_t>(b2) << 8) |
         static_cast<uint32_t>(b3);
  } else {
    w2 = encode_W2(ac_mode, health, timer_on);
  }

  uint8_t checksum = compute_checksum(w0, w1, w2);

  static const uint8_t NBITS = 104;

  uint32_t tRawData[4] = {
      w0,
      w1,
      w2,
      static_cast<uint32_t>(checksum),
  };

  ESP_LOGD(TAG, "Sending frame: W0=0x%08X W1=0x%08X W2=0x%08X C=0x%02X",
           w0, w1, w2, checksum);

  // 1) Ask the transmitter for a transmit call
  auto tx = this->transmitter_->transmit();

  // 2) Get the underlying RemoteTransmitData*
  auto *data = tx.get_data();

  // 3) Fill it using your helper
  sendPulseDistanceWidthFromArray(
      data,
      38,        // carrier kHz
      8950,      // header_mark
      4450,      // header_space
      550,       // one_mark
      1650,      // one_space
      550,       // zero_mark
      550,       // zero_space
      tRawData,
      NBITS,
      BitOrder::LSB_FIRST);

  // 4) Actually send it
  tx.perform();
}

void BaymakACComponent::send_known_good_test_() {
  if (this->transmitter_ == nullptr) return;

  const uint32_t tRawData[4] = {
      0x00E0B7C3,
      0x00800060,
      0x05003000,
      0x0000006F,  // checksum byte in LSB
  };

  auto tx = this->transmitter_->transmit();
  auto *data = tx.get_data();

  sendPulseDistanceWidthFromArray(
      data,
      38,   // carrier kHz
      9000, // header_mark (same as before)
      4500, // header_space
      550,  // one_mark
      1650, // one_space
      550,  // zero_mark
      550,  // zero_space
      tRawData,
      105,  // nbits
      BitOrder::LSB_FIRST);

  tx.perform();
}

}  // namespace baymak_ac_ns
}  // namespace esphome

