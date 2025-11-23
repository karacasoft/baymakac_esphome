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
};
}  // namespace baymak_ac_ns
}  // namespace esphome
