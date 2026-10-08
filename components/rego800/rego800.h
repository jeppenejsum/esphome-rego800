#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <map>
#include <string>
#include <vector>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace rego800 {

enum Rego800SensorType {
  REGULAR,
  THERMISTOR,
};

class Rego800 : public Component {
public:
#ifdef USE_SENSOR
  struct SensorInfo {
    sensor::Sensor *sensor;
    Rego800SensorType type;
    SensorInfo(sensor::Sensor *s, Rego800SensorType t) : sensor(s), type(t) {}
    SensorInfo() : sensor(nullptr), type(REGULAR) {}
  };
#endif

#ifdef USE_TEXT_SENSOR
  struct TextSensorInfo {
    text_sensor::TextSensor *text_sensor;
    std::map<int, std::string> mapping;
    TextSensorInfo(text_sensor::TextSensor *s, std::map<int, std::string> m)
        : text_sensor(s), mapping(std::move(m)) {}
    TextSensorInfo() : text_sensor(nullptr) {}
  };
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;

  void on_frame(uint32_t can_id, bool rtr, const std::vector<uint8_t> &data);

  void set_ignore_ids(std::vector<uint32_t> ids) {
    this->ignore_ids_ = std::move(ids);
  }
  void set_sniff(bool sniff) { this->sniff_ = sniff; }
  // Log every unmapped CAN ID seen so far with its last payload.
  void dump_sniff();

  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  // Read-only scan of the controller's variable table: send a remote request
  // for each variable in [first, last]. The first scan only records values;
  // later scans log the variables whose value changed.
  void start_scan(uint16_t first, uint16_t last, uint32_t interval_ms = 50);
  // Log every recorded scan value, rate limited so the log stream keeps up.
  void dump_scan();
  // Request the controller's variable-name table (var 0x7F6, which streams
  // many frames) and log it as parsed entries once it has arrived.
  void read_names();

#ifdef USE_SENSOR
  void register_sensor(uint32_t can_id, sensor::Sensor *sensor,
                       Rego800SensorType type = REGULAR) {
    this->sensors_[can_id] = SensorInfo(sensor, type);
  }
#endif

#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(uint32_t can_id,
                              binary_sensor::BinarySensor *binary_sensor) {
    this->binary_sensors_[can_id] = binary_sensor;
  }
#endif

#ifdef USE_TEXT_SENSOR
  void register_text_sensor(uint32_t can_id,
                            text_sensor::TextSensor *text_sensor) {
    this->text_sensors_[can_id] = TextSensorInfo(text_sensor, {});
  }
  void add_text_sensor_mapping(uint32_t can_id, int val, std::string text) {
    if (this->text_sensors_.count(can_id)) {
      this->text_sensors_[can_id].mapping[val] = std::move(text);
    }
  }
#endif

protected:
#ifdef USE_SENSOR
  std::map<uint32_t, SensorInfo> sensors_;
#endif

#ifdef USE_BINARY_SENSOR
  std::map<uint32_t, binary_sensor::BinarySensor *> binary_sensors_;
#endif

#ifdef USE_TEXT_SENSOR
  std::map<uint32_t, TextSensorInfo> text_sensors_;
#endif

  std::vector<uint32_t> ignore_ids_;

  // Sniff mode: log unmapped CAN IDs when first seen and whenever their
  // payload changes, to help decode the protocol.
  bool is_mapped_(uint32_t can_id) const;
  void sniff_frame_(uint32_t can_id, bool rtr, const std::vector<uint8_t> &data);
  bool sniff_{false};
  std::map<uint32_t, std::vector<uint8_t>> sniff_last_;

  // Variable requests and their replies share one ID:
  // 1 << 26 | variable << 14 | node 0x3FE0.
  static const uint32_t REQUEST_BASE = 0x04003FE0;
  static const uint32_t REQUEST_MASK = 0x1C003FFF;
  static const uint16_t SCAN_VARS = 0x800;
  static const uint16_t NAMES_INFO_VAR = 0x7F5;
  static const uint16_t NAMES_VAR = 0x7F6;
  static const size_t NAMES_BUF_SIZE = 16384;
  struct ScanEntry {
    uint8_t len;  // 0xFF = no reply yet
    uint8_t data[8];
  };
  void handle_scan_reply_(uint16_t var, const std::vector<uint8_t> &data);

  canbus::Canbus *canbus_{nullptr};
  ScanEntry *scan_table_{nullptr};
  bool scanning_{false};
  uint16_t scan_next_{0};
  uint16_t scan_last_{0};
  uint32_t scan_interval_ms_{50};
  uint32_t scan_last_send_ms_{0};
  uint16_t scan_replies_{0};
  uint16_t scan_changes_{0};
  bool scan_first_{true};

  void finish_names_();
  bool print_scan_line_();
  bool print_name_line_();

  uint8_t *names_buf_{nullptr};
  size_t names_len_{0};
  size_t names_dropped_{0};
  bool names_capturing_{false};
  uint32_t names_started_ms_{0};
  uint32_t names_last_rx_ms_{0};

  // Rate-limited output of a scan dump or the parsed name table.
  enum PrintMode { PRINT_NONE, PRINT_SCAN, PRINT_NAMES };
  PrintMode print_mode_{PRINT_NONE};
  size_t print_pos_{0};
  uint32_t print_last_ms_{0};
};

} // namespace rego800
} // namespace esphome
