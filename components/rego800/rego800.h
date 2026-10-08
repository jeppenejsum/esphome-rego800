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
  void dump_config() override;

  void on_frame(uint32_t can_id, bool rtr, const std::vector<uint8_t> &data);

  void set_ignore_ids(std::vector<uint32_t> ids) {
    this->ignore_ids_ = std::move(ids);
  }
  void set_sniff(bool sniff) { this->sniff_ = sniff; }
  // Log every unmapped CAN ID seen so far with its last payload.
  void dump_sniff();

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
};

} // namespace rego800
} // namespace esphome
