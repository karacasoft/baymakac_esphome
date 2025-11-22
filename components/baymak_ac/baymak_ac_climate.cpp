#include "esphome/core/log.h"
#include "baymak_ac_climate.hpp"

namespace esphome {
namespace baymak_ac_ns {
static const char *TAG = "baymak_ac_component.component";

void BaymakACComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
  this->pin_->setup();
  this->pin_->digital_write(false);
}

void BaymakACComponent::loop() {

}


void BaymakACComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Baymak AC Component");
  LOG_PIN(" Pin: ", this->pin_);
}

}  // namespace baymak_ac_ns
}  // namespace esphome
