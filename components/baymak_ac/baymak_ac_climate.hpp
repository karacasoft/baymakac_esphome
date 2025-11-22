#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace baymak_ac_ns {
class BaymakACComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_led_pin(GPIOPin *pin) { this->pin_ = pin; }

 protected:
  GPIOPin *pin_;
};
}  // namespace baymak_ac_ns
}  // namespace esphome
