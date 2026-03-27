import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import alde3020_ns, Alde3020Component, CONF_ALDE3020_ID

DEPENDENCIES = ["alde3020"]

Alde3020Switch = alde3020_ns.class_("Alde3020Switch", switch.Switch, cg.Component)

CONF_GAS_ENABLE  = "gas_enable"
CONF_PANEL_ON    = "panel_on"
CONF_AC_AUTO     = "ac_auto"

SWITCH_TYPES = {
    CONF_GAS_ENABLE: "GAS_ENABLE",
    CONF_PANEL_ON:   "PANEL_ON",
    CONF_AC_AUTO:    "AC_AUTO",
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

    for key, type_str in SWITCH_TYPES.items():
        if key not in config:
            continue
        sw_conf = config[key]
        var = await switch.new_switch(sw_conf)
        await cg.register_component(var, sw_conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_switch_type(
            getattr(alde3020_ns.class_("Alde3020Switch"), type_str)
        ))
