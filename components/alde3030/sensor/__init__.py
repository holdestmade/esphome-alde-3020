import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from .. import alde3030_ns, Alde3030Component, CONF_ALDE3030_ID

DEPENDENCIES = ["alde3030"]

Alde3030Sensor = alde3030_ns.class_("Alde3030Sensor", cg.Component)

CONF_ZONE1_TEMPERATURE   = "zone1_temperature"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_ZONE1_TARGET        = "zone1_target_temperature"
CONF_ELECTRIC_POWER      = "electric_power_kw"

TEMP_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Alde3030Sensor),
        cv.GenerateID(CONF_ALDE3030_ID): cv.use_id(Alde3030Component),
        cv.Optional(CONF_ZONE1_TEMPERATURE):   TEMP_SENSOR_SCHEMA,
        cv.Optional(CONF_OUTDOOR_TEMPERATURE): TEMP_SENSOR_SCHEMA,
        cv.Optional(CONF_ZONE1_TARGET):        TEMP_SENSOR_SCHEMA,
        cv.Optional(CONF_ELECTRIC_POWER): sensor.sensor_schema(
            unit_of_measurement="kW",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALDE3030_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_parent(parent))

    for key, setter in [
        (CONF_ZONE1_TEMPERATURE,   "set_zone1_temperature_sensor"),
        (CONF_OUTDOOR_TEMPERATURE, "set_outdoor_temperature_sensor"),
        (CONF_ZONE1_TARGET,        "set_zone1_target_sensor"),
        (CONF_ELECTRIC_POWER,      "set_electric_power_sensor"),
    ]:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))
