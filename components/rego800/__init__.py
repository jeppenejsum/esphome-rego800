
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@jeppenejsum"]
DEPENDENCIES = ["canbus"]

rego800_ns = cg.esphome_ns.namespace("rego800")
Rego800 = rego800_ns.class_("Rego800", cg.Component)
Rego800SensorType = rego800_ns.enum("Rego800SensorType")

CONF_REGO800_ID = "rego800_id"
CONF_CANBUS_ID = "canbus_id"
CONF_IGNORE_IDS = "ignore_ids"
CONF_SNIFF = "sniff"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Rego800),
        cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_IGNORE_IDS): cv.ensure_list(cv.hex_uint32_t),
        cv.Optional(CONF_SNIFF, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    canbus_component = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(canbus_component))
    # The callback receives: (uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data)
    callback = cg.RawExpression(
        f"""[=](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {{
            id({config[CONF_ID]}).on_frame(can_id, rtr, data);
        }}"""
    )
    cg.add(canbus_component.add_callback(callback))
    
    if CONF_IGNORE_IDS in config:
        cg.add(var.set_ignore_ids(config[CONF_IGNORE_IDS]))

    if config[CONF_SNIFF]:
        cg.add(var.set_sniff(True))
