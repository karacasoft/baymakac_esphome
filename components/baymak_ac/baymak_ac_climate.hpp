#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace baymak_ac_ns {

class BaymakACComponent : public climate::Climate, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Climate
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

 protected:
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};

 private:
  void send_frame_();

  // Remember last non-OFF state so we can send a deterministic OFF frame
  climate::ClimateMode last_mode_{climate::CLIMATE_MODE_COOL};
  climate::ClimateFanMode last_fan_mode_{climate::CLIMATE_FAN_AUTO};
  climate::ClimateSwingMode last_swing_mode_{climate::CLIMATE_SWING_OFF};
  float last_target_temperature_{24.0f};
};

}  // namespace baymak_ac_ns
}  // namespace esphome

