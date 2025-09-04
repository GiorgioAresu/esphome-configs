import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, binary_sensor, output
from esphome.const import CONF_ID

DEPENDENCIES = ["output"]
AUTO_LOAD = ["fan", "binary_sensor", "output", "logger"]

fan_controller_ns = cg.esphome_ns.namespace("fan_controller")
FanController = fan_controller_ns.class_("FanController", fan.Fan, cg.Component)

CONF_POWER_OUTPUT = "power_output"
CONF_SPEED_OUTPUT = "speed_output"
CONF_LOW_LED = "low_led"
CONF_HIGH_LED = "high_led"

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(FanController),
    cv.Required(CONF_POWER_OUTPUT): cv.use_id(output.BinaryOutput),
    cv.Required(CONF_SPEED_OUTPUT): cv.use_id(output.BinaryOutput),
    cv.Required(CONF_LOW_LED): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(CONF_HIGH_LED): cv.use_id(binary_sensor.BinarySensor),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await fan.register_fan(var, config)

    power = await cg.get_variable(config[CONF_POWER_OUTPUT])
    cg.add(var.set_power_output(power))
    
    speed = await cg.get_variable(config[CONF_SPEED_OUTPUT])
    cg.add(var.set_speed_output(speed))
    
    low_led = await cg.get_variable(config[CONF_LOW_LED])
    cg.add(var.set_low_led(low_led))
    
    high_led = await cg.get_variable(config[CONF_HIGH_LED])
    cg.add(var.set_high_led(high_led))