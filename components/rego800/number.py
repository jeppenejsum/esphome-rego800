import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ADDRESS,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_SIZE,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    UNIT_CELSIUS,
)

from . import CONF_REGO800_ID, Rego800, rego800_ns

DEPENDENCIES = ["rego800"]

Rego800Number = rego800_ns.class_(
    "Rego800Number", number.Number, cg.Parented.template(Rego800)
)

CONF_REGO_VARIABLE = "rego_variable"
CONF_SIGNED = "signed"
CONF_MULTIPLIER = "multiplier"

# Addresses and sizes come from the controller's own name table
# (docs/rego800-2.21.0-variables.txt) and may differ on other firmware.
# Scale and ranges are inferred from observed values, not from IVT docs.
def _temp(address, min_value, max_value, signed=False):
    return {
        CONF_ADDRESS: address,
        CONF_SIZE: 2,
        CONF_SIGNED: signed,
        CONF_MULTIPLIER: 0.1,
        CONF_MIN_VALUE: min_value,
        CONF_MAX_VALUE: max_value,
        CONF_STEP: 0.1,
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
    }


REGO_VARIABLES = {
    # Heating season limit, 1 byte in whole °C. Confirmed by changing it on
    # the panel and by writing it.
    "VARMESASONG_TEMP": {
        CONF_ADDRESS: 0x2BC,
        CONF_SIZE: 1,
        CONF_SIGNED: False,
        CONF_MULTIPLIER: 1.0,
        CONF_MIN_VALUE: 10,
        CONF_MAX_VALUE: 25,
        CONF_STEP: 1,
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
    },
    # Heat curve end points (flow temperature, x0.1 °C).
    "RADKURVA_VANSTER_Y": _temp(0x275, 15, 50),
    "RADKURVA_HOGER_Y": _temp(0x271, 25, 80),
    # Local curve adjustments, signed x0.1 °C, for outdoor points -35 (Y1) to
    # +20 °C (Y12) in 5 °C steps. The panel shows end point + adjustment, e.g.
    # left end 29.4 + Y12 -10.0 = 19 at +20 °C.
    **{
        f"RADKURVA_Y{i}": _temp(0x278 + 2 * (i - 1), -15, 15, signed=True)
        for i in range(1, 13)
    },
}


def _apply_preset(config):
    if (CONF_REGO_VARIABLE in config) == (CONF_ADDRESS in config):
        raise cv.Invalid("Specify exactly one of rego_variable or address")
    if CONF_REGO_VARIABLE in config:
        for key, value in REGO_VARIABLES[config[CONF_REGO_VARIABLE]].items():
            config.setdefault(key, value)
    else:
        for key in (CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP):
            if key not in config:
                raise cv.Invalid(f"'{key}' is required with 'address'")
        config.setdefault(CONF_SIZE, 1)
        config.setdefault(CONF_SIGNED, False)
        config.setdefault(CONF_MULTIPLIER, 1.0)
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(Rego800Number).extend(
        {
            cv.GenerateID(CONF_REGO800_ID): cv.use_id(Rego800),
            cv.Optional(CONF_REGO_VARIABLE): cv.one_of(*REGO_VARIABLES, upper=True),
            cv.Optional(CONF_ADDRESS): cv.int_range(min=0, max=0xFFF),
            cv.Optional(CONF_SIZE): cv.one_of(1, 2, 4, int=True),
            cv.Optional(CONF_SIGNED): cv.boolean,
            cv.Optional(CONF_MULTIPLIER): cv.positive_float,
            cv.Optional(CONF_MIN_VALUE): cv.float_,
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP): cv.positive_float,
        }
    ),
    _apply_preset,
)


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    rego = await cg.get_variable(config[CONF_REGO800_ID])
    cg.add(var.set_parent(rego))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_size(config[CONF_SIZE]))
    cg.add(var.set_signed(config[CONF_SIGNED]))
    cg.add(var.set_multiplier(config[CONF_MULTIPLIER]))
    cg.add(rego.register_number(var))
