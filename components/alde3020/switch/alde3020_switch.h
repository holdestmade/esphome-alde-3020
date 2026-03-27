#pragma once

#include "esphome/components/switch/switch.h"
#include "../alde3020.h"

namespace esphome {
namespace alde3020 {

class Alde3020Switch : public switch_::Switch, public Component {
 public:
  enum SwitchType { GAS_ENABLE, PANEL_ON, AC_AUTO };

  void set_parent(Alde3020Component *p)   { parent_ = p; }
  void set_switch_type(SwitchType t)      { type_   = t; }

  void setup() override {
    parent_->add_on_state_callback([this]() { this->sync_(); });
  }

 protected:
  void write_state(bool state) override {
    switch (type_) {
      case GAS_ENABLE: parent_->set_gas_enabled(state); break;
      case PANEL_ON:   parent_->set_panel_on(state);    break;
      case AC_AUTO:    parent_->set_ac_auto(state);     break;
    }
    publish_state(state);
  }

  void sync_() {
    bool s = false;
    switch (type_) {
      case GAS_ENABLE: s = parent_->get_gas_active();  break;
      case PANEL_ON:   s = parent_->get_panel_on();    break;
      case AC_AUTO:    s = parent_->get_ac_auto();     break;
    }
    publish_state(s);
  }

  Alde3020Component *parent_{nullptr};
  SwitchType         type_{PANEL_ON};
};

}  // namespace alde3020
}  // namespace esphome
