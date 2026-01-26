# ESPHome Rego800 Component

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESPHome](https://img.shields.io/badge/ESPHome-Component-blue.svg)](https://esphome.io/)

An ESPHome external component for reading data from **Rego 800** heat pump controllers via CAN bus. This controller is used in heat pumps from manufacturers like **IVT**, **Bosch**, **Autotterm**, and others.

## Features

- 📊 Read temperature sensors (T1-T11) with automatic thermistor conversion
- ⚡ Read pump and compressor frequencies
- 🔘 Binary sensors for pump/fan status
- 📝 Text sensors with value mapping (e.g., 3-way valve position)
- 🔧 Easily extensible with custom CAN IDs
- 🏠 Full Home Assistant integration

## Supported Heat Pumps

This component has been tested with:
- IVT PremiumLine X15

May work with other heat pumps using the Rego 800 controller family.

## Hardware Requirements

- **ESP32** development board
- **CAN bus transceiver** (e.g., SN65HVD230, MCP2551, or integrated like T-CAN485)
- Connection to the heat pump's CAN bus (check your heat pump's documentation)

Example board: [T-CAN485](https://github.com/Xinyuan-LilyGO/T-CAN485) - connects to both power and CAN bus in the heat pump.

> ⚠️ **Warning**: Connecting to your heat pump's CAN bus may void your warranty. Proceed at your own risk.

## Installation

### Option 1: GitHub (Recommended)

Add to your ESPHome configuration:

```yaml
external_components:
  - source: github://jeppenejsum/esphome-rego800
    components: [rego800]
```

### Option 2: Local

1. Copy the `esphome-rego800` folder to your ESPHome config directory
2. Reference it locally:

```yaml
external_components:
  - source: esphome-rego800/components
    components: [rego800]
```

## Quick Start

```yaml
# CAN bus configuration
canbus:
  - platform: esp32_can
    id: my_canbus
    tx_pin: GPIO27
    rx_pin: GPIO26
    can_id: 4
    use_extended_id: true
    bit_rate: 125KBPS

# Rego800 component
rego800:
  canbus_id: my_canbus
  ignore_ids: [0x9ffc030, 0x9ffc040]  # Optional: filter noisy CAN IDs

# Example sensors
sensor:
  - platform: rego800
    rego_variable: T1
    name: "Supply Flow Temperature"

  - platform: rego800
    rego_variable: COMPRESSOR
    name: "Compressor Frequency"

binary_sensor:
  - platform: rego800
    rego_variable: HEAT_FLUID_PUMP_CONTROL
    name: "Heat Pump Running"

text_sensor:
  - platform: rego800
    rego_variable: THREEWAY_VALVE
    name: "3-Way Valve"
```

See [example.yaml](example.yaml) for a complete configuration.

## Custom CAN IDs

If you need to read a CAN ID not included in the predefined variables:

```yaml
sensor:
  - platform: rego800
    can_id: 0x12345678
    type: THERMISTOR  # or REGULAR
    name: "Custom Sensor"
    unit_of_measurement: "°C"
    accuracy_decimals: 1
```

## Available Variables

### Sensors (sensor.py)

| Variable | CAN ID | Type | Description |
|----------|--------|------|-------------|
| T1 | 0x10000040 | Thermistor | Supply flow temperature |
| T2 | 0x10004040 | Thermistor | Outdoor temperature |
| T3 | 0x10008040 | Thermistor | Hot water temperature |
| T6 | 0x1000c040 | Thermistor | Hot gas temperature |
| T8 | 0x10010040 | Thermistor | Heat carrier out |
| T9 | 0x10014040 | Thermistor | Heat carrier in |
| T10 | 0x10018040 | Thermistor | Cold carrier in |
| T11 | 0x1001c040 | Thermistor | Cold carrier out |
| HEAT_FLUID_PUMP | 0x8070040 | Regular | Heat fluid pump frequency |
| COLD_FLUID_PUMP | 0x8074040 | Regular | Cold fluid pump frequency |
| COMPRESSOR | 0x80bc040 | Regular | Compressor frequency |

### Binary Sensors (binary_sensor.py)

| Variable | CAN ID | Description |
|----------|--------|-------------|
| HEAT_FLUID_PUMP_CONTROL | 0x8040040 | Heat fluid pump on/off |
| COLD_FLUID_PUMP_CONTROL | 0x8044040 | Cold fluid pump on/off |
| COOLING_FAN | 0x8048040 | Cooling fan on/off |

### Text Sensors (text_sensor.py)

| Variable | CAN ID | Description |
|----------|--------|-------------|
| THREEWAY_VALVE | 0x804c040 | 3-way valve position |

## Troubleshooting

1. **No data received**: Check your CAN bus wiring and ensure the bit rate matches (125 KBPS)
2. **Incorrect temperatures**: Verify you're using the correct sensor type (THERMISTOR vs REGULAR)
3. **Missing sensors**: Some CAN IDs may vary between heat pump models

Enable debug logging to see raw CAN frames:
```yaml
logger:
  level: DEBUG
```

## Contributing

Contributions are welcome! If you've discovered additional CAN IDs or support for other heat pump models, please open a pull request.

### Adding New Sensors

1. Add the CAN ID and configuration to the appropriate Python file (`sensor.py`, `binary_sensor.py`, or `text_sensor.py`)
2. Test with your heat pump
3. Submit a PR with your changes

## License

MIT License - See [LICENSE](LICENSE) file for details.

## Acknowledgments

- ESPHome team for the excellent framework
- The heat pump reverse engineering community