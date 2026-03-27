#pragma once

#include "esphome/components/climate/climate.h"
#include "../alde3020.h"

namespace esphome {
namespace alde3020 {

class Alde3020Climate : public climate::Climate, public Component {
 public:
  void set_parent(Alde3020Component *parent) { parent_ = parent; }
  void set_zone(uint8_t zone) { zone_ = zone; }

  void setup() override {
    parent_->add_on_state_callback([this]() { this->on_heater_update_(); });
  }

  climate::ClimateTraits traits() override {
    auto t = climate::ClimateTraits();
    t.add_supported_mode(climate::CLIMATE_MODE_HEAT);
    t.add_supported_mode(climate::CLIMATE_MODE_OFF);
    t.add_feature_flags(climate::CLIMATE_FLAG_SUPPORTS_CURRENT_TEMPERATURE);
    t.set_visual_min_temperature(5.0f);
    t.set_visual_max_temperature(30.0f);
    t.set_visual_temperature_step(0.5f);
    return t;
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      auto mode = *call.get_mode();
      this->mode = mode;
      if (mode == climate::CLIMATE_MODE_OFF) {
        parent_->set_panel_on(false);
      } else {
        parent_->set_panel_on(true);
      }
    }
    if (call.get_target_temperature().has_value()) {
      float t = *call.get_target_temperature();
      this->target_temperature = t;
      if (zone_ == 1) parent_->set_zone1_target(t);
      else            parent_->set_zone2_target(t);
    }
    this->publish_state();
  }

 protected:
  void on_heater_update_() {
    if (zone_ == 1) {
      this->current_temperature = parent_->get_zone1_actual();
      this->target_temperature  = parent_->get_zone1_setpoint();
    } else {
      this->current_temperature = parent_->get_zone2_actual();
      this->target_temperature  = parent_->get_zone2_setpoint();
    }
    this->mode = parent_->get_panel_on()
                     ? climate::CLIMATE_MODE_HEAT
                     : climate::CLIMATE_MODE_OFF;
    this->publish_state();
  }

  Alde3020Component *parent_{nullptr};
  uint8_t zone_{1};
};

}  // namespace alde3020
}  // namespace esphome
