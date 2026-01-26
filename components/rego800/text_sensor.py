
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from esphome.const import (
    CONF_ID,
    CONF_OPTIONS,
)
from . import Rego800, CONF_REGO800_ID, rego800_ns

DEPENDENCIES = ["rego800"]

CONF_REGO_VARIABLE = "rego_variable"
CONF_CAN_ID = "can_id"

REGO_VARIABLES = {
    "THREEWAY_VALVE": {CONF_CAN_ID: 0x804c040, CONF_OPTIONS: {0: "Varme", 1: "Varmt vand"}},
}

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_REGO800_ID): cv.use_id(Rego800),
        cv.Optional(CONF_REGO_VARIABLE): cv.enum(REGO_VARIABLES),
        cv.Optional(CONF_CAN_ID): cv.hex_uint32_t,
        cv.Optional(CONF_OPTIONS): cv.Schema({cv.int_: cv.string})
    }
).extend(cv.COMPONENT_SCHEMA)

def validate_config(config):
    if CONF_REGO_VARIABLE not in config and CONF_CAN_ID not in config:
        raise cv.Invalid("Must specify either rego_variable or can_id")
    if CONF_REGO_VARIABLE in config and CONF_CAN_ID in config:
        raise cv.Invalid("Cannot specify both rego_variable and can_id")
    
    if CONF_REGO_VARIABLE in config:
        var_data = REGO_VARIABLES[config[CONF_REGO_VARIABLE]]
        if CONF_OPTIONS in var_data and CONF_OPTIONS not in config:
            config[CONF_OPTIONS] = var_data[CONF_OPTIONS]
            
    if CONF_OPTIONS not in config:
        config[CONF_OPTIONS] = {}
        
    return config

FINAL_VALIDATE_SCHEMA = validate_config

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    
    rego = await cg.get_variable(config[CONF_REGO800_ID])
    
    can_id = 0
    if CONF_REGO_VARIABLE in config:
        can_id = REGO_VARIABLES[config[CONF_REGO_VARIABLE]][CONF_CAN_ID]
    else:
        can_id = config[CONF_CAN_ID]
        
    cg.add(rego.register_text_sensor(can_id, var))
    for val, text in config[CONF_OPTIONS].items():
        cg.add(rego.add_text_sensor_mapping(can_id, val, text))
