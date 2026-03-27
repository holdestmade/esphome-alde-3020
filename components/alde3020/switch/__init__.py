import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import alde3020_ns, Alde3020Component, CONF_ALDE3020_ID

DEPENDENCIES = ["alde3020"]

Alde3020Switch = alde3020_ns.class_("Alde3020Switch", switch.Switch, cg.Component)
Alde3020SwitchType = alde3020_ns.enum("Alde3020SwitchType")

CONF_GAS_ENABLE  = "gas_enable"
CONF_PANEL_ON    = "panel_on"
CONF_AC_AUTO     = "ac_auto"

SWITCH_TYPES = {
    CONF_GAS_ENABLE: Alde3020SwitchType.ALDE3020_SWITCH_GAS_ENABLE,
    CONF_PANEL_ON:   Alde3020SwitchType.ALDE3020_SWITCH_PANEL_ON,
    CONF_AC_AUTO:    Alde3020SwitchType.ALDE3020_SWITCH_AC_AUTO,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALDE3020_ID): cv.use_id(Alde3020Component),
        cv.Optional(CONF_GAS_ENABLE):  switch.switch_schema(Alde3020Switch),
        cv.Optional(CONF_PANEL_ON):    switch.switch_schema(Alde3020Switch),
        cv.Optional(CONF_AC_AUTO):     switch.switch_schema(Alde3020Switch),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3020_ID])
    for key, switch_type in SWITCH_TYPES.items():
        if key not in config:
            continue
        sw_conf = config[key]
        var = await switch.new_switch(sw_conf)
        await cg.register_component(var, sw_conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_switch_type(switch_type))
