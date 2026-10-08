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
  ESP_LOGCONFIG(TAG, "  Sniff mode: %s", YESNO(this->sniff_));
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

bool Rego800::is_mapped_(uint32_t can_id) const {
#ifdef USE_SENSOR
  if (this->sensors_.count(can_id))
    return true;
#endif
#ifdef USE_BINARY_SENSOR
  if (this->binary_sensors_.count(can_id))
    return true;
#endif
#ifdef USE_TEXT_SENSOR
  if (this->text_sensors_.count(can_id))
    return true;
#endif
  return false;
}

// Field split follows the Rego 1000 layout (type << 26 | variable << 14 |
// node); it is a working hypothesis for the Rego 800, not a confirmed spec.
void Rego800::sniff_frame_(uint32_t can_id, bool rtr,
                           const std::vector<uint8_t> &data) {
  uint32_t type = (can_id >> 26) & 0x7;
  uint32_t var = (can_id >> 14) & 0xFFF;
  uint32_t node = can_id & 0x3FFF;

  if (rtr) {
    ESP_LOGI(TAG, "SNIFF RTR 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32,
             can_id, type, var, node);
    return;
  }

  char hex[3 * 8 + 1] = {0};
  for (size_t i = 0; i < data.size() && i < 8; i++)
    snprintf(hex + i * 3, 4, "%02X ", data[i]);

  auto it = this->sniff_last_.find(can_id);
  if (it == this->sniff_last_.end()) {
    if (this->sniff_last_.size() >= 512)
      return;
    this->sniff_last_[can_id] = data;
    ESP_LOGI(TAG, "SNIFF NEW 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32 " len=%u data=%s",
             can_id, type, var, node, (unsigned) data.size(), hex);
  } else if (it->second != data) {
    char old_hex[3 * 8 + 1] = {0};
    for (size_t i = 0; i < it->second.size() && i < 8; i++)
      snprintf(old_hex + i * 3, 4, "%02X ", it->second[i]);
    ESP_LOGI(TAG, "SNIFF CHG 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32 " %s-> %s",
             can_id, type, var, node, old_hex, hex);
    it->second = data;
  }
}

void Rego800::dump_sniff() {
  if (!this->sniff_) {
    ESP_LOGW(TAG, "Sniff mode is off; set 'sniff: true' to collect CAN IDs");
    return;
  }
  ESP_LOGI(TAG, "SNIFF DUMP: %u IDs", (unsigned) this->sniff_last_.size());
  for (auto const &it : this->sniff_last_) {
    char hex[3 * 8 + 1] = {0};
    for (size_t i = 0; i < it.second.size() && i < 8; i++)
      snprintf(hex + i * 3, 4, "%02X ", it.second[i]);
    ESP_LOGI(TAG, "SNIFF ID 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32 " len=%u data=%s",
             it.first, (it.first >> 26) & 0x7, (it.first >> 14) & 0xFFF,
             it.first & 0x3FFF, (unsigned) it.second.size(), hex);
  }
}

void Rego800::on_frame(uint32_t can_id, bool rtr,
                       const std::vector<uint8_t> &data) {
  if (std::find(this->ignore_ids_.begin(), this->ignore_ids_.end(), can_id) !=
      this->ignore_ids_.end()) {
    return;
  }

  if (this->sniff_ && !this->is_mapped_(can_id))
    this->sniff_frame_(can_id, rtr, data);

  if (rtr)
    return;

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
