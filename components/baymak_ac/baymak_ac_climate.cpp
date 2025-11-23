#include "esphome/core/log.h"
#include "esphome/components/remote_base/remote_base.h"
#include "baymak_ac_climate.hpp"
#include "protocol.hpp"
#include <string>

namespace esphome {
namespace baymak_ac_ns {
static const char *TAG = "baymak_ac_component.component";

void BaymakACComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
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
      climate::CLIMATE_MODE_AUTO,
  });

  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });

  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1.0f);

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
    this->fan_mode = *call.get_fan_mode();
  }

  // TODO: map mode/temperature/fan_mode into your bitfields here
  // and use that inside send_frame_()

  // Send IR once
  this->send_frame_();

  // Optional: repeat once after 200ms (common for AC remotes)
  this->set_timeout(200, [this]() { this->send_frame_(); });

  // Report new state to Home Assistant
  this->publish_state();
}

void BaymakACComponent::send_frame_() {
  static const uint32_t tRawData[] = {
    0xE0B7C3, 0x800060, 0x05003000, 0x6F
  };

  // 1) Ask the transmitter for a transmit call
  auto tx = this->transmitter_->transmit();

  // 2) Get the underlying RemoteTransmitData*
  auto *data = tx.get_data();

  // 3) Fill it using your helper
  sendPulseDistanceWidthFromArray(
      data,
      38,        // carrier kHz
      9000,      // header_mark
      4500,      // header_space
      550,       // one_mark
      1650,      // one_space
      550,       // zero_mark
      550,       // zero_space
      tRawData,
      104,       // nbits
      BitOrder::LSB_FIRST,
      true
  );

  // 4) Actually send it
  tx.perform();

}

}  // namespace baymak_ac_ns
}  // namespace esphome
