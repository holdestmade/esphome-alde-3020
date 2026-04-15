import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID

from .. import alde3030_ns, Alde3030Component, CONF_ALDE3030_ID

DEPENDENCIES = ["alde3030"]

Alde3030Climate = alde3030_ns.class_(
    "Alde3030Climate", climate.Climate, cg.Component
)

CONFIG_SCHEMA = (
    climate.climate_schema(Alde3030Climate)
    .extend(
        {
            cv.GenerateID(CONF_ALDE3030_ID): cv.use_id(Alde3030Component),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3030_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    cg.add(var.set_parent(parent))
