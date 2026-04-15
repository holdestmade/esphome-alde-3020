#pragma once

#include "esphome/components/switch/switch.h"
#include "../alde3030.h"

namespace esphome {
namespace alde3030 {

enum Alde3030SwitchType { ALDE3030_SWITCH_GAS_ENABLE, ALDE3030_SWITCH_PANEL_ON };

class Alde3030Switch : public switch_::Switch, public Component {
 public:
  void set_parent(Alde3030Component *p)          { parent_ = p; }
  void set_switch_type(Alde3030SwitchType type)  { type_   = type; }

  void setup() override {
    parent_->add_on_state_callback([this]() { this->sync_(); });
  }

 protected:
  void write_state(bool state) override {
    switch (type_) {
      case ALDE3030_SWITCH_GAS_ENABLE: parent_->set_gas_enabled(state); break;
      case ALDE3030_SWITCH_PANEL_ON:   parent_->set_panel_on(state);    break;
    }
    publish_state(state);
  }

  void sync_() {
    bool s = false;
    switch (type_) {
      case ALDE3030_SWITCH_GAS_ENABLE: s = parent_->get_gas_active(); break;
      case ALDE3030_SWITCH_PANEL_ON:   s = parent_->get_panel_on();   break;
    }
    publish_state(s);
  }

  Alde3030Component *parent_{nullptr};
  Alde3030SwitchType type_{ALDE3030_SWITCH_PANEL_ON};
};

}  // namespace alde3030
}  // namespace esphome
