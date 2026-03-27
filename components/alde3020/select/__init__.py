import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from .. import alde3020_ns, Alde3020Component, CONF_ALDE3020_ID

DEPENDENCIES = ["alde3020"]

Alde3020Select = alde3020_ns.class_("Alde3020Select", select.Select, cg.Component)

CONF_ELECTRIC_POWER = "electric_power"
CONF_WATER_MODE     = "water_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALDE3020_ID): cv.use_id(Alde3020Component),
        cv.Optional(CONF_ELECTRIC_POWER): select.select_schema(Alde3020Select),
        cv.Optional(CONF_WATER_MODE):     select.select_schema(Alde3020Select),
    }
)

ELECTRIC_OPTIONS = ["Off", "1 kW", "2 kW", "3 kW"]
WATER_OPTIONS    = ["Off", "Normal", "Boost", "Auto"]


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3020_ID])

    if CONF_ELECTRIC_POWER in config:
        var = await select.new_select(config[CONF_ELECTRIC_POWER], options=ELECTRIC_OPTIONS)
        await cg.register_component(var, config[CONF_ELECTRIC_POWER])
        cg.add(var.set_parent(parent))
        cg.add(var.set_select_type(alde3020_ns.class_("Alde3020Select").ELECTRIC_POWER))

    if CONF_WATER_MODE in config:
        var = await select.new_select(config[CONF_WATER_MODE], options=WATER_OPTIONS)
        await cg.register_component(var, config[CONF_WATER_MODE])
        cg.add(var.set_parent(parent))
        cg.add(var.set_select_type(alde3020_ns.class_("Alde3020Select").WATER_MODE))
