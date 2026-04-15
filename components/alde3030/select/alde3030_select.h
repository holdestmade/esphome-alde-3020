#pragma once

#include "esphome/components/select/select.h"
#include "../alde3030.h"

namespace esphome {
namespace alde3030 {

enum Alde3030SelectType { ALDE3030_SELECT_ELECTRIC_POWER, ALDE3030_SELECT_WATER_MODE };

class Alde3030Select : public select::Select, public Component {
 public:
  static const char *ELECTRIC_OPTIONS[4];
  // 3030 Plus: Off / On / Boost  (no Auto mode)
  static const char *WATER_OPTIONS[3];

  void set_parent(Alde3030Component *p)           { parent_ = p; }
  void set_select_type(Alde3030SelectType type)   { type_   = type; }

  void setup() override {
    parent_->add_on_state_callback([this]() { this->sync_(); });
  }

 protected:
  void control(const std::string &value) override {
    if (type_ == ALDE3030_SELECT_ELECTRIC_POWER) {
      for (uint8_t i = 0; i < 4; i++) {
        if (value == ELECTRIC_OPTIONS[i]) { parent_->set_electric_power(i); break; }
      }
    } else {
      for (uint8_t i = 0; i < 3; i++) {
        if (value == WATER_OPTIONS[i]) { parent_->set_water_mode(i); break; }
      }
    }
    publish_state(value);
  }

  void sync_() {
    if (type_ == ALDE3030_SELECT_ELECTRIC_POWER) {
      uint8_t kw = parent_->get_electric_kw();
      if (kw < 4) publish_state(ELECTRIC_OPTIONS[kw]);
    } else {
      uint8_t m = parent_->get_water_mode();
      if (m < 3) publish_state(WATER_OPTIONS[m]);
    }
  }

  Alde3030Component *parent_{nullptr};
  Alde3030SelectType type_{ALDE3030_SELECT_ELECTRIC_POWER};
};

const char *Alde3030Select::ELECTRIC_OPTIONS[4] = {"Off", "1 kW", "2 kW", "3 kW"};
const char *Alde3030Select::WATER_OPTIONS[3]    = {"Off", "On", "Boost"};

}  // namespace alde3030
}  // namespace esphome
