#pragma once

#include "esphome/components/select/select.h"
#include "../alde3020.h"

namespace esphome {
namespace alde3020 {

enum Alde3020SelectType { ALDE3020_SELECT_ELECTRIC_POWER, ALDE3020_SELECT_WATER_MODE };

class Alde3020Select : public select::Select, public Component {
 public:
  static const char *ELECTRIC_OPTIONS[4];
  static const char *WATER_OPTIONS[4];

  void set_parent(Alde3020Component *p)             { parent_ = p; }
  void set_select_type(Alde3020SelectType type)     { type_   = type; }

  void setup() override {
    parent_->add_on_state_callback([this]() { this->sync_(); });
  }

 protected:
  void control(const std::string &value) override {
    if (type_ == ALDE3020_SELECT_ELECTRIC_POWER) {
      for (uint8_t i = 0; i < 4; i++) {
        if (value == ELECTRIC_OPTIONS[i]) { parent_->set_electric_power(i); break; }
      }
    } else {
      for (uint8_t i = 0; i < 4; i++) {
        if (value == WATER_OPTIONS[i]) { parent_->set_water_mode(i); break; }
      }
    }
    publish_state(value);
  }

  void sync_() {
    if (type_ == ALDE3020_SELECT_ELECTRIC_POWER) {
      uint8_t kw = parent_->get_electric_kw();
      if (kw < 4) publish_state(ELECTRIC_OPTIONS[kw]);
    } else {
      uint8_t m = parent_->get_water_mode();
      if (m < 4) publish_state(WATER_OPTIONS[m]);
    }
  }

  Alde3020Component *parent_{nullptr};
  Alde3020SelectType type_{ALDE3020_SELECT_ELECTRIC_POWER};
};

const char *Alde3020Select::ELECTRIC_OPTIONS[4] = {"Off", "1 kW", "2 kW", "3 kW"};
const char *Alde3020Select::WATER_OPTIONS[4]    = {"Off", "Normal", "Boost", "Auto"};

}  // namespace alde3020
}  // namespace esphome
