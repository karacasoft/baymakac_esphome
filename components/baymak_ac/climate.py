from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN
from esphome.cpp_helpers import gpio_pin_expression
from esphome.components import remote_transmitter, climate

CONF_TRANSMITTER_ID = "transmitter_id"

baymak_ac_ns = cg.esphome_ns.namespace("baymak_ac_ns")
BaymakACComponent = baymak_ac_ns.class_("BaymakACComponent", climate.Climate, cg.Component)

CONFIG_SCHEMA = climate._CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BaymakACComponent),
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(remote_transmitter.RemoteTransmitterComponent)
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    tx = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(tx))

    await climate.register_climate(var, config)
    await cg.register_component(var, config)
