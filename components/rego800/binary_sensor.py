
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_DEVICE_CLASS,
    DEVICE_CLASS_PROBLEM,
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
    # 1-byte flags read on the rego800 poll_interval (name table flags 00);
    # non-zero = on. Meaning inferred from the names only.
    "KOMP_LARM": {CONF_ADDRESS: 0x244, CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM},
    "LARM_MODE": {CONF_ADDRESS: 0x24C, CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM},
    "FRYSVAKT": {CONF_ADDRESS: 0x172, CONF_DEVICE_CLASS: DEVICE_CLASS_PROBLEM},
}

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_REGO800_ID): cv.use_id(Rego800),
        cv.Optional(CONF_REGO_VARIABLE): cv.enum(REGO_VARIABLES),
        cv.Optional(CONF_CAN_ID): cv.hex_uint32_t,
        cv.Optional(CONF_ADDRESS): cv.int_range(min=0, max=0xFFF),
    }
).extend(cv.COMPONENT_SCHEMA)

def validate_config(config):
    sources = [k for k in (CONF_REGO_VARIABLE, CONF_CAN_ID, CONF_ADDRESS) if k in config]
    if len(sources) != 1:
        raise cv.Invalid("Specify exactly one of rego_variable, can_id or address")
    
    if CONF_REGO_VARIABLE in config:
        var_data = REGO_VARIABLES[config[CONF_REGO_VARIABLE]]
        for key in [CONF_DEVICE_CLASS, CONF_ADDRESS]:
            if key in var_data and key not in config:
                config[key] = var_data[key]
            
    return config

FINAL_VALIDATE_SCHEMA = validate_config

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    
    rego = await cg.get_variable(config[CONF_REGO800_ID])

    address = config.get(CONF_ADDRESS)
    if CONF_REGO_VARIABLE in config:
        address = REGO_VARIABLES[config[CONF_REGO_VARIABLE]].get(CONF_ADDRESS, address)
    if address is not None:
        cg.add(rego.register_polled_binary_sensor(address, var))
        return
    
    can_id = 0
    if CONF_REGO_VARIABLE in config:
        can_id = REGO_VARIABLES[config[CONF_REGO_VARIABLE]][CONF_CAN_ID]
    else:
        can_id = config[CONF_CAN_ID]
        
    cg.add(rego.register_binary_sensor(can_id, var))
