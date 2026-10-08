#include "rego800.h"
#include "esphome/core/log.h"
#include "rego800_lut.h"
#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace rego800 {

static const char *const TAG = "rego800";

// Formats up to 8 bytes as "AA BB CC "; out must hold 25 chars.
static void format_payload(const uint8_t *data, size_t len, char *out) {
  out[0] = '\0';
  for (size_t i = 0; i < len && i < 8; i++)
    snprintf(out + i * 3, 4, "%02X ", data[i]);
}

void Rego800::setup() {
  // Setup logic if needed. Callback is registered via Python add_callback.
}

void Rego800::start_scan(uint16_t first, uint16_t last, uint32_t interval_ms) {
  if (this->canbus_ == nullptr) {
    ESP_LOGW(TAG, "SCAN: no canbus configured");
    return;
  }
  if (this->scanning_) {
    ESP_LOGW(TAG, "SCAN: already running");
    return;
  }
  last = std::min<uint16_t>(last, SCAN_VARS - 1);
  if (first > last)
    return;
  if (this->scan_table_ == nullptr) {
    this->scan_table_ = new ScanEntry[SCAN_VARS];
    for (uint16_t i = 0; i < SCAN_VARS; i++)
      this->scan_table_[i].len = 0xFF;
    this->scan_first_ = true;
  }
  this->scan_next_ = first;
  this->scan_last_ = last;
  this->scan_interval_ms_ = interval_ms;
  this->scan_replies_ = 0;
  this->scan_changes_ = 0;
  this->scanning_ = true;
  ESP_LOGI(TAG, "SCAN start: vars 0x%03X-0x%03X, %" PRIu32 " ms apart", first,
           last, interval_ms);
}

void Rego800::loop() {
  uint32_t now = millis();

  if (this->names_capturing_) {
    bool got_data = this->names_len_ + this->names_dropped_ > 0;
    if ((got_data && now - this->names_last_rx_ms_ > 1000) ||
        (!got_data && now - this->names_started_ms_ > 3000))
      this->finish_names_();
  }

  if (this->print_mode_ != PRINT_NONE && now - this->print_last_ms_ >= 20) {
    this->print_last_ms_ = now;
    for (int i = 0; i < 4; i++) {
      bool more = this->print_mode_ == PRINT_SCAN ? this->print_scan_line_()
                                                  : this->print_name_line_();
      if (!more) {
        this->print_mode_ = PRINT_NONE;
        break;
      }
    }
  }

  if (!this->scanning_ || now - this->scan_last_send_ms_ < this->scan_interval_ms_)
    return;

  // The name table streams many frames per request; read_names() handles it.
  if (this->scan_next_ == NAMES_VAR)
    this->scan_next_++;

  // One extra interval after the last request lets its reply arrive.
  if (this->scan_next_ > this->scan_last_) {
    this->scanning_ = false;
    if (this->scan_first_) {
      ESP_LOGI(TAG, "SCAN done: %u variables replied (baseline recorded; "
                    "press Dump scan to list them)",
               this->scan_replies_);
    } else {
      ESP_LOGI(TAG, "SCAN done: %u replies, %u changed", this->scan_replies_,
               this->scan_changes_);
    }
    this->scan_first_ = false;
    return;
  }

  uint32_t can_id = REQUEST_BASE | (uint32_t(this->scan_next_) << 14);
  if (this->canbus_->send_data(can_id, true, true, {}) != canbus::ERROR_OK) {
    this->scanning_ = false;
    ESP_LOGW(TAG, "SCAN aborted at var 0x%03X: send failed", this->scan_next_);
    return;
  }
  this->scan_last_send_ms_ = now;
  this->scan_next_++;
}

void Rego800::dump_scan() {
  if (this->scan_table_ == nullptr) {
    ESP_LOGW(TAG, "SCAN: nothing recorded yet; press Scan variables first");
    return;
  }
  ESP_LOGI(TAG, "SCAN DUMP start");
  this->print_mode_ = PRINT_SCAN;
  this->print_pos_ = 0;
}

bool Rego800::print_scan_line_() {
  while (this->print_pos_ < SCAN_VARS &&
         this->scan_table_[this->print_pos_].len == 0xFF)
    this->print_pos_++;
  if (this->print_pos_ >= SCAN_VARS) {
    ESP_LOGI(TAG, "SCAN DUMP end");
    return false;
  }
  const ScanEntry &entry = this->scan_table_[this->print_pos_];
  char hex[25];
  format_payload(entry.data, entry.len, hex);
  uint32_t value = 0;
  for (size_t i = 0; i < entry.len && i < 4; i++)
    value = (value << 8) | entry.data[i];
  ESP_LOGD(TAG, "SCAN var=0x%03X len=%u data=%s= %" PRIu32,
           (unsigned) this->print_pos_, entry.len, hex, value);
  this->print_pos_++;
  return true;
}

void Rego800::read_names() {
  if (this->canbus_ == nullptr || this->names_capturing_)
    return;
  if (this->names_buf_ == nullptr)
    this->names_buf_ = new uint8_t[NAMES_BUF_SIZE];
  this->names_len_ = 0;
  this->names_dropped_ = 0;
  this->names_capturing_ = true;
  this->names_started_ms_ = millis();
  this->print_mode_ = PRINT_NONE;
  ESP_LOGI(TAG, "NAMES: requesting variable-name table");
  this->canbus_->send_data(REQUEST_BASE | (uint32_t(NAMES_INFO_VAR) << 14),
                           true, true, {});
  this->canbus_->send_data(REQUEST_BASE | (uint32_t(NAMES_VAR) << 14), true,
                           true, {});
}

void Rego800::finish_names_() {
  this->names_capturing_ = false;
  ESP_LOGI(TAG, "NAMES: received %u bytes%s", (unsigned) this->names_len_,
           this->names_dropped_ ? " (buffer full, rest dropped)" : "");
  if (this->names_len_ > 0) {
    this->print_mode_ = PRINT_NAMES;
    this->print_pos_ = 0;
  }
}

// Entry layout, inferred from the stream: 2-byte index (big endian), one flag
// byte, then the NUL-terminated name.
bool Rego800::print_name_line_() {
  size_t pos = this->print_pos_;
  if (pos + 3 >= this->names_len_) {
    ESP_LOGI(TAG, "NAMES end");
    return false;
  }
  const uint8_t *buf = this->names_buf_;
  size_t end = pos + 3;
  while (end < this->names_len_ && end - pos < 67 && buf[end] != 0 &&
         buf[end] >= 0x20 && buf[end] < 0x7F)
    end++;
  if (end >= this->names_len_ || buf[end] != 0) {
    char hex[25];
    format_payload(buf + pos, std::min<size_t>(8, this->names_len_ - pos), hex);
    ESP_LOGW(TAG, "NAMES parse stopped at offset %u: %s", (unsigned) pos, hex);
    return false;
  }
  uint16_t index = (buf[pos] << 8) | buf[pos + 1];
  ESP_LOGD(TAG, "NAME 0x%03X flags=%02X %.*s", index, buf[pos + 2],
           (int) (end - pos - 3), (const char *) buf + pos + 3);
  this->print_pos_ = end + 1;
  return true;
}

void Rego800::handle_scan_reply_(uint16_t var,
                                 const std::vector<uint8_t> &data) {
  this->scan_replies_++;
  ScanEntry &entry = this->scan_table_[var];
  size_t len = std::min<size_t>(data.size(), 8);
  bool is_new = entry.len == 0xFF;
  if (!is_new && entry.len == len &&
      std::equal(data.begin(), data.begin() + len, entry.data))
    return;

  char hex[25], old_hex[25];
  format_payload(data.data(), len, hex);
  uint32_t value = 0;
  for (size_t i = 0; i < len && i < 4; i++)
    value = (value << 8) | data[i];

  if (is_new) {
    if (!this->scan_first_)
      ESP_LOGD(TAG, "SCAN NEW var=0x%03X len=%u data=%s= %" PRIu32, var,
               (unsigned) len, hex, value);
  } else {
    format_payload(entry.data, entry.len, old_hex);
    ESP_LOGD(TAG, "SCAN CHG var=0x%03X %s-> %s= %" PRIu32, var, old_hex, hex,
             value);
  }
  this->scan_changes_++;
  entry.len = len;
  std::copy(data.begin(), data.begin() + len, entry.data);
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
    ESP_LOGD(TAG, "SNIFF RTR 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32,
             can_id, type, var, node);
    return;
  }

  char hex[25];
  format_payload(data.data(), data.size(), hex);

  auto it = this->sniff_last_.find(can_id);
  if (it == this->sniff_last_.end()) {
    if (this->sniff_last_.size() >= 512)
      return;
    this->sniff_last_[can_id] = data;
    ESP_LOGD(TAG, "SNIFF NEW 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32 " len=%u data=%s",
             can_id, type, var, node, (unsigned) data.size(), hex);
  } else if (it->second != data) {
    char old_hex[25];
    format_payload(it->second.data(), it->second.size(), old_hex);
    ESP_LOGD(TAG, "SNIFF CHG 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
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
  ESP_LOGI(TAG, "SNIFF DUMP start: %u IDs", (unsigned) this->sniff_last_.size());
  for (auto const &it : this->sniff_last_) {
    char hex[25];
    format_payload(it.second.data(), it.second.size(), hex);
    ESP_LOGD(TAG, "SNIFF ID 0x%08" PRIX32 " type=%" PRIu32 " var=0x%03" PRIX32
                  " node=0x%04" PRIX32 " len=%u data=%s",
             it.first, (it.first >> 26) & 0x7, (it.first >> 14) & 0xFFF,
             it.first & 0x3FFF, (unsigned) it.second.size(), hex);
  }
  ESP_LOGI(TAG, "SNIFF DUMP end");
}

void Rego800::on_frame(uint32_t can_id, bool rtr,
                       const std::vector<uint8_t> &data) {
  if (std::find(this->ignore_ids_.begin(), this->ignore_ids_.end(), can_id) !=
      this->ignore_ids_.end()) {
    return;
  }

  if (!rtr && (can_id & REQUEST_MASK) == REQUEST_BASE) {
    uint16_t var = (can_id >> 14) & 0xFFF;
    if (var == NAMES_VAR) {
      if (this->names_capturing_) {
        for (uint8_t b : data) {
          if (this->names_len_ < NAMES_BUF_SIZE)
            this->names_buf_[this->names_len_++] = b;
          else
            this->names_dropped_++;
        }
        this->names_last_rx_ms_ = millis();
      }
      return;
    }
    if (var == NAMES_INFO_VAR && this->names_capturing_ && data.size() >= 6) {
      ESP_LOGI(TAG, "NAMES info: %u bytes, %u entries (if 0x7F5 means that)",
               (data[2] << 8) | data[3], (data[4] << 8) | data[5]);
    }
    if (this->scan_table_ != nullptr && var < SCAN_VARS) {
      this->handle_scan_reply_(var, data);
      return;
    }
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
