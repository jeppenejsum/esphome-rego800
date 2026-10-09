# ESPHome Rego800 Component

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESPHome](https://img.shields.io/badge/ESPHome-Component-blue.svg)](https://esphome.io/)

An ESPHome external component for reading data from **Rego 800** heat pump controllers via CAN bus. This controller is used in heat pumps from manufacturers like **IVT**, **Bosch**, **Autotterm**, and others.

## Features

- 📊 Read temperature sensors (T1-T11) with automatic thermistor conversion
- ⚡ Read pump and compressor frequencies
- 🔘 Binary sensors for pump/fan status
- 📝 Text sensors with value mapping (e.g., 3-way valve position)
- 📈 Read controller variables that are not broadcast (flow setpoint, compressor drive, alarms)
- 🎛️ Read and write controller settings (heating season limit, heat curve) as number entities
- 🔍 Tools for decoding the bus: sniff mode, variable scan and the controller's own name table
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

### Polled controller variables

These are not broadcast; they are read from the controller on `poll_interval`
(see [Settings](#settings-numberpy) for the request scheme), and need the ESP
to transmit. Other variables can be used with `address`, `size`, `signed` and
`multiplier` (sensor) or `address` (binary sensor, non-zero = on).

| Variable | Platform | Address | Format | Description |
|----------|----------|---------|--------|-------------|
| RAD_BORVARDE | sensor | 0x25E | 2 bytes signed, ×0.1 °C | Calculated flow temperature setpoint |
| HW_KOMP_CURRENT | sensor | 0x20B | 2 bytes, ×0.1 A | Compressor drive current (0 when stopped, ~8.5 A at 48 Hz). Whether this is DC-bus or motor current is not confirmed, so V × A is not a verified power figure. |
| HW_KOMP_VOLTAGE | sensor | 0x21D | 2 bytes, V | Inverter DC-bus voltage (~566 V idle, sags ~15 V under load) |
| KOMP_LARM | binary_sensor | 0x244 | 1 byte | Compressor alarm |
| LARM_MODE | binary_sensor | 0x24C | 1 byte | Alarm mode |
| FRYSVAKT | binary_sensor | 0x172 | 1 byte | Freeze protection active |

The alarm meanings are inferred from the variable names; they read 0 with no
alarm on the panel, but have not been seen in the on state.

## Settings (number.py)

Besides the broadcast values above, the controller answers requests for its
internal variables, and accepts writes to them. Variables are byte addresses:
a remote request on `1 << 26 | address << 14 | 0x3FE0` is answered with a data
frame on the same ID, and a data frame sent to that ID writes the value.

Writing requires the ESP to transmit. On a T-CAN485 the transceiver only
receives until GPIO23 is driven low:

```yaml
switch:
  - platform: gpio
    pin: GPIO23          # transceiver speed/standby pin: LOW = can transmit
    internal: true
    restore_mode: ALWAYS_OFF
  - platform: gpio
    pin: GPIO16          # ME2107 boost supply for the CAN side
    internal: true
    restore_mode: ALWAYS_ON

rego800:
  canbus_id: my_canbus
  poll_interval: 10s     # how often settings are re-read (default 60s)

number:
  - platform: rego800
    rego_variable: VARMESASONG_TEMP
    name: "Heating season limit"
  - platform: rego800
    rego_variable: RADKURVA_VANSTER_Y
    name: "Heat curve left end"
```

Each number is re-read on `poll_interval`, so changes made on the panel show
up in Home Assistant. A change from Home Assistant is written once and read
back a second later; writes of an unchanged value are skipped, since the
controller stores settings in non-volatile memory with limited write cycles.

| Variable | Address | Format | Description |
|----------|---------|--------|-------------|
| VARMESASONG_TEMP | 0x2BC | 1 byte, °C | Heating season limit |
| RADKURVA_VANSTER_Y | 0x275 | 2 bytes, ×0.1 °C | Heat curve left end (+20 °C outdoor) |
| RADKURVA_HOGER_Y | 0x271 | 2 bytes, ×0.1 °C | Heat curve right end (−35 °C outdoor) |
| RADKURVA_Y1 … Y12 | 0x278 … 0x28E | 2 bytes signed, ×0.1 °C | Local curve adjustments at −35 … +20 °C outdoor, 5 °C apart |

The panel shows a curve point as end point plus adjustment: a left end of 29.4
with Y12 at −10.0 reads as 19 at +20 °C. Addresses come from firmware 2.21.0
and may differ on other versions. Other variables can be used with `address`,
`size`, `signed`, `multiplier`, `min_value`, `max_value` and `step`.

## Decoding tools

With `sniff: true` the component logs CAN IDs that no sensor uses, when first
seen and whenever their payload changes. It also exposes these methods for
template buttons:

- `id(rego).dump_sniff()` lists every unmapped ID seen so far.
- `id(rego).start_scan(0x000, 0x7FF)` reads every controller variable. The
  first scan records a baseline; later scans log only the variables that
  changed, so changing one setting on the panel between two scans finds it.
- `id(rego).dump_scan()` lists the recorded scan values.
- `id(rego).read_names()` reads the controller's variable-name table (var
  0x7F6) and logs it. It arrives as a burst of ~1700 frames, so set
  `rx_queue_len: 64` on the `canbus`.
- `id(rego).poll_now()` re-reads all number entities immediately.

[docs/rego800-2.21.0-variables.txt](docs/rego800-2.21.0-variables.txt) holds
the name table read from firmware 2.21.0.

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