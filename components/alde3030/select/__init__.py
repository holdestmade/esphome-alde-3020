import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from .. import alde3030_ns, Alde3030Component, CONF_ALDE3030_ID

DEPENDENCIES = ["alde3030"]

Alde3030Select = alde3030_ns.class_("Alde3030Select", select.Select, cg.Component)
Alde3030SelectType = alde3030_ns.enum("Alde3030SelectType")

CONF_ELECTRIC_POWER = "electric_power"
CONF_WATER_MODE     = "water_mode"

# 3030 Plus water modes: Off / On / Boost  (no Auto; mode value 3 maps to On)
ELECTRIC_OPTIONS = ["Off", "1 kW", "2 kW", "3 kW"]
WATER_OPTIONS    = ["Off", "On", "Boost"]

SELECT_TYPES = {
    CONF_ELECTRIC_POWER: Alde3030SelectType.ALDE3030_SELECT_ELECTRIC_POWER,
    CONF_WATER_MODE:     Alde3030SelectType.ALDE3030_SELECT_WATER_MODE,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALDE3030_ID): cv.use_id(Alde3030Component),
        cv.Optional(CONF_ELECTRIC_POWER): select.select_schema(Alde3030Select),
        cv.Optional(CONF_WATER_MODE):     select.select_schema(Alde3030Select),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3030_ID])

    if CONF_ELECTRIC_POWER in config:
        var = await select.new_select(config[CONF_ELECTRIC_POWER], options=ELECTRIC_OPTIONS)
        await cg.register_component(var, config[CONF_ELECTRIC_POWER])
        cg.add(var.set_parent(parent))
        cg.add(var.set_select_type(SELECT_TYPES[CONF_ELECTRIC_POWER]))

    if CONF_WATER_MODE in config:
        var = await select.new_select(config[CONF_WATER_MODE], options=WATER_OPTIONS)
        await cg.register_component(var, config[CONF_WATER_MODE])
        cg.add(var.set_parent(parent))
        cg.add(var.set_select_type(SELECT_TYPES[CONF_WATER_MODE]))
