import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "climate", "switch", "select"]

alde3020_ns = cg.esphome_ns.namespace("alde3020")
Alde3020Component = alde3020_ns.class_(
    "Alde3020Component", cg.Component, uart.UARTDevice
)

CONF_ALDE3020_ID = "alde3020_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Alde3020Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
