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
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#include <deque>

namespace esphome {
namespace rego800 {

enum Rego800SensorType {
  REGULAR,
  THERMISTOR,
};

#ifdef USE_NUMBER
class Rego800Number;
#endif

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

  // Controller variables are byte addresses; see docs/ for the name table.
  void request_read(uint16_t address);
  void write_variable(uint16_t address, uint8_t size, int32_t raw);
  void set_poll_interval(uint32_t ms) { this->poll_interval_ms_ = ms; }
  // Re-read all number entities now instead of waiting for the next poll.
  void poll_now() { this->poll_last_ms_ = millis() - this->poll_interval_ms_; }
#ifdef USE_NUMBER
  void register_number(Rego800Number *number) {
    this->numbers_.push_back(number);
  }
#endif
  // Parse a big-endian controller value of size 1, 2 or 4 bytes.
  static bool parse_value(const std::vector<uint8_t> &data, uint8_t size,
                          bool is_signed, int32_t &raw);
#ifdef USE_SENSOR
  // A read-only controller variable, re-read on the poll interval.
  void register_polled_sensor(uint16_t address, uint8_t size, bool is_signed,
                              float multiplier, sensor::Sensor *sensor) {
    PolledVariable v;
    v.address = address;
    v.size = size;
    v.is_signed = is_signed;
    v.multiplier = multiplier;
    v.sensor = sensor;
    this->polled_.push_back(v);
  }
#endif
#ifdef USE_BINARY_SENSOR
  void register_polled_binary_sensor(uint16_t address,
                                     binary_sensor::BinarySensor *sensor) {
    PolledVariable v;
    v.address = address;
    v.binary_sensor = sensor;
    this->polled_.push_back(v);
  }
#endif

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

  // Reads queued by request_read(), sent one at a time between scan requests.
  std::deque<uint16_t> read_queue_;
  uint32_t read_last_send_ms_{0};
  uint32_t poll_interval_ms_{60000};
  uint32_t poll_last_ms_{0};
  bool polled_once_{false};
#ifdef USE_NUMBER
  std::vector<Rego800Number *> numbers_;
#endif
  struct PolledVariable {
    uint16_t address{0};
    uint8_t size{1};
    bool is_signed{false};
    float multiplier{1.0f};
#ifdef USE_SENSOR
    sensor::Sensor *sensor{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
    binary_sensor::BinarySensor *binary_sensor{nullptr};
#endif
  };
  std::vector<PolledVariable> polled_;
  void handle_polled_reply_(uint16_t address, const std::vector<uint8_t> &data);
};

#ifdef USE_NUMBER
// A writable controller variable. The value is read on the poll interval; a
// change from Home Assistant is written once and then read back, so the state
// always reflects what the controller reports.
class Rego800Number : public number::Number, public Parented<Rego800> {
public:
  void set_address(uint16_t address) { this->address_ = address; }
  void set_size(uint8_t size) { this->size_ = size; }
  void set_signed(bool is_signed) { this->signed_ = is_signed; }
  void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }
  uint16_t get_address() const { return this->address_; }

  void handle_reply(const std::vector<uint8_t> &data);

protected:
  void control(float value) override;

  uint16_t address_{0};
  uint8_t size_{1};
  bool signed_{false};
  float multiplier_{1.0f};
  bool has_raw_{false};
  int32_t raw_{0};
};
#endif

} // namespace rego800
} // namespace esphome
