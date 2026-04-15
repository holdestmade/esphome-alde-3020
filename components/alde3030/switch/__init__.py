import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import alde3030_ns, Alde3030Component, CONF_ALDE3030_ID

DEPENDENCIES = ["alde3030"]

Alde3030Switch = alde3030_ns.class_("Alde3030Switch", switch.Switch, cg.Component)
Alde3030SwitchType = alde3030_ns.enum("Alde3030SwitchType")

CONF_GAS_ENABLE = "gas_enable"
CONF_PANEL_ON   = "panel_on"

SWITCH_TYPES = {
    CONF_GAS_ENABLE: Alde3030SwitchType.ALDE3030_SWITCH_GAS_ENABLE,
    CONF_PANEL_ON:   Alde3030SwitchType.ALDE3030_SWITCH_PANEL_ON,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALDE3030_ID): cv.use_id(Alde3030Component),
        cv.Optional(CONF_GAS_ENABLE): switch.switch_schema(Alde3030Switch),
        cv.Optional(CONF_PANEL_ON):   switch.switch_schema(Alde3030Switch),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3030_ID])
    for key, switch_type in SWITCH_TYPES.items():
        if key not in config:
            continue
        sw_conf = config[key]
        var = await switch.new_switch(sw_conf)
        await cg.register_component(var, sw_conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_switch_type(switch_type))
