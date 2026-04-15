#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../alde3030.h"

namespace esphome {
namespace alde3030 {

class Alde3030Sensor : public Component {
 public:
  void set_parent(Alde3030Component *p) { parent_ = p; }

  void set_zone1_temperature_sensor(sensor::Sensor *s)   { zone1_temp_   = s; }
  void set_outdoor_temperature_sensor(sensor::Sensor *s) { outdoor_temp_ = s; }
  void set_zone1_target_sensor(sensor::Sensor *s)        { zone1_target_ = s; }
  void set_electric_power_sensor(sensor::Sensor *s)      { elec_power_   = s; }

  void setup() override {
    parent_->add_on_state_callback([this]() { this->publish_(); });
  }

 protected:
  void publish_() {
    if (zone1_temp_   && !std::isnan(parent_->get_zone1_actual()))
      zone1_temp_->publish_state(parent_->get_zone1_actual());
    if (outdoor_temp_ && !std::isnan(parent_->get_outdoor_actual()))
      outdoor_temp_->publish_state(parent_->get_outdoor_actual());
    if (zone1_target_)
      zone1_target_->publish_state(parent_->get_zone1_setpoint());
    if (elec_power_)
      elec_power_->publish_state((float)parent_->get_electric_kw());
  }

  Alde3030Component *parent_{nullptr};
  sensor::Sensor    *zone1_temp_{nullptr};
  sensor::Sensor    *outdoor_temp_{nullptr};
  sensor::Sensor    *zone1_target_{nullptr};
  sensor::Sensor    *elec_power_{nullptr};
};

}  // namespace alde3030
}  // namespace esphome
