import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID, CONF_NAME

from .. import alde3020_ns, Alde3020Component, CONF_ALDE3020_ID

DEPENDENCIES = ["alde3020"]

Alde3020Climate = alde3020_ns.class_("Alde3020Climate", climate.Climate, cg.Component)

CONF_ZONE = "zone"

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Alde3020Climate),
        cv.GenerateID(CONF_ALDE3020_ID): cv.use_id(Alde3020Component),
        cv.Optional(CONF_ZONE, default=1): cv.int_range(min=1, max=2),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3020_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    cg.add(var.set_parent(parent))
    cg.add(var.set_zone(config[CONF_ZONE]))
