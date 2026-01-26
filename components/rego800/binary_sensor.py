
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from esphome.const import (
    CONF_ID,
    CONF_DEVICE_CLASS,
    DEVICE_CLASS_RUNNING,
)
from . import Rego800, CONF_REGO800_ID, rego800_ns

DEPENDENCIES = ["rego800"]

CONF_REGO_VARIABLE = "rego_variable"
CONF_CAN_ID = "can_id"

REGO_VARIABLES = {
    "HEAT_FLUID_PUMP_CONTROL": {CONF_CAN_ID: 0x8040040, CONF_DEVICE_CLASS: DEVICE_CLASS_RUNNING},
    "COLD_FLUID_PUMP_CONTROL": {CONF_CAN_ID: 0x8044040, CONF_DEVICE_CLASS: DEVICE_CLASS_RUNNING},
    "COOLING_FAN": {CONF_CAN_ID: 0x8048040, CONF_DEVICE_CLASS: DEVICE_CLASS_RUNNING},
}

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_REGO800_ID): cv.use_id(Rego800),
        cv.Optional(CONF_REGO_VARIABLE): cv.enum(REGO_VARIABLES),
        cv.Optional(CONF_CAN_ID): cv.hex_uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)

def validate_config(config):
    if CONF_REGO_VARIABLE not in config and CONF_CAN_ID not in config:
        raise cv.Invalid("Must specify either rego_variable or can_id")
    if CONF_REGO_VARIABLE in config and CONF_CAN_ID in config:
        raise cv.Invalid("Cannot specify both rego_variable and can_id")
    
    if CONF_REGO_VARIABLE in config:
        var_data = REGO_VARIABLES[config[CONF_REGO_VARIABLE]]
        if CONF_DEVICE_CLASS in var_data and CONF_DEVICE_CLASS not in config:
            config[CONF_DEVICE_CLASS] = var_data[CONF_DEVICE_CLASS]
            
    return config

FINAL_VALIDATE_SCHEMA = validate_config

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    
    rego = await cg.get_variable(config[CONF_REGO800_ID])
    
    can_id = 0
    if CONF_REGO_VARIABLE in config:
        can_id = REGO_VARIABLES[config[CONF_REGO_VARIABLE]][CONF_CAN_ID]
    else:
        can_id = config[CONF_CAN_ID]
        
    cg.add(rego.register_binary_sensor(can_id, var))
