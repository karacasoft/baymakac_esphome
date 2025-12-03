#pragma once

#include "esphome/components/climate/climate_mode.h"
#include "esphome/core/component.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/hal.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace baymak_ac_ns {

struct ACStoredState {
  bool powered;
  climate::ClimateMode mode;
  float target_temperature;
  climate::ClimateFanMode fan_mode;
  climate::ClimateSwingMode swing_mode;
};

class BaymakACComponent : public climate::Climate, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Climate
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void send_known_good_test_();

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

 protected:
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  ESPPreferenceObject pref_;
  ACStoredState stored_state_{};

 private:
  void send_frame_();

  // Remember last non-OFF state so we can send a deterministic OFF frame
  climate::ClimateMode last_mode_{climate::CLIMATE_MODE_COOL};
  optional<climate::ClimateFanMode> last_fan_mode_{climate::CLIMATE_FAN_AUTO};
  climate::ClimateSwingMode last_swing_mode_{climate::CLIMATE_SWING_OFF};
  float last_target_temperature_{24.0f};
};

}  // namespace baymak_ac_ns
}  // namespace esphome

