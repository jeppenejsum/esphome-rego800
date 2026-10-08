#include "rego800.h"
#include "esphome/core/log.h"
#include "rego800_lut.h"
#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace rego800 {

static const char *const TAG = "rego800";

void Rego800::setup() {
  // Setup logic if needed. Callback is registered via Python add_callback.
}

void Rego800::dump_config() {
  ESP_LOGCONFIG(TAG, "Rego800:");
  if (!this->ignore_ids_.empty()) {
    ESP_LOGCONFIG(TAG, "  Ignored CAN IDs:");
    for (auto id : this->ignore_ids_) {
      ESP_LOGCONFIG(TAG, "    - 0x%08" PRIX32, id);
    }
  }

#ifdef USE_SENSOR
  for (auto const &it : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor 0x%08" PRIX32 ": %s (%s)", it.first,
                  it.second.sensor->get_name().c_str(),
                  it.second.type == THERMISTOR ? "Thermistor" : "Regular");
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto const &it : this->binary_sensors_) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor 0x%08" PRIX32 ": %s", it.first,
                  it.second->get_name().c_str());
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto const &it : this->text_sensors_) {
    ESP_LOGCONFIG(TAG, "  Text Sensor 0x%08" PRIX32 ": %s", it.first,
                  it.second.text_sensor->get_name().c_str());
  }
#endif
}

void Rego800::on_frame(uint32_t can_id, bool rtr,
                       const std::vector<uint8_t> &data) {
  if (rtr)
    return;

  if (std::find(this->ignore_ids_.begin(), this->ignore_ids_.end(), can_id) !=
      this->ignore_ids_.end()) {
    return;
  }

  uint8_t dlc = data.size();
  ESP_LOGV(TAG, "Received frame 0x%08" PRIX32 " DLC=%u", can_id, dlc);

  if (dlc == 1) {
    uint8_t val = data[0];

#ifdef USE_SENSOR
    if (this->sensors_.count(can_id)) {
      this->sensors_[can_id].sensor->publish_state(val);
    }
#endif

#ifdef USE_BINARY_SENSOR
    if (this->binary_sensors_.count(can_id)) {
      this->binary_sensors_[can_id]->publish_state(val != 0);
    }
#endif

#ifdef USE_TEXT_SENSOR
    if (this->text_sensors_.count(can_id)) {
      auto &info = this->text_sensors_[can_id];
      if (info.mapping.count(val)) {
        info.text_sensor->publish_state(info.mapping[val]);
      } else {
        info.text_sensor->publish_state(std::to_string(val));
      }
    }
#endif
  } else if (dlc == 2) {
    uint16_t raw_val = (uint16_t(data[0]) << 8) | data[1];

#ifdef USE_SENSOR
    if (this->sensors_.count(can_id)) {
      auto &info = this->sensors_[can_id];
      if (info.type == THERMISTOR) {
        if (raw_val < 1024) {
          float temp = TEMP_LOOKUP[raw_val];
          if (temp >= -30.0f && temp <= 100.0f) {
            info.sensor->publish_state(temp);
          } else {
            info.sensor->publish_state(NAN);
          }
        }
      } else {
        info.sensor->publish_state(raw_val);
      }
    }
#endif
  }
}

} // namespace rego800
} // namespace esphome
