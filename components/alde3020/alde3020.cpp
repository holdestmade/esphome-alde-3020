#include "alde3020.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>
#include <cstdio>

namespace esphome {
namespace alde3020 {

// ── LIN PID: add parity bits (P0, P1) per LIN spec ────────────────────────
uint8_t Alde3020Component::lin_pid_(uint8_t id) {
  uint8_t p0 = ((id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 1;
  uint8_t p1 = (~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5))) & 1;
  return (id & 0x3F) | (p0 << 6) | (p1 << 7);
}

// ── LIN checksum ───────────────────────────────────────────────────────────
uint8_t Alde3020Component::lin_checksum_(uint8_t pid, const uint8_t *data, size_t len, bool include_pid) {
  uint16_t sum = include_pid ? pid : 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
    if (sum > 0xFF) sum -= 0xFF;
  }
  return (uint8_t)(~sum & 0xFF);
}

// ── Send a LIN break: hold TX low for >13 bit-times ──────────────────────
// Most LIN-to-TTL modules handle the break automatically when you configure
// the UART for 13-bit break detection, but many cheap modules just pass
// bytes through. We emulate a break by briefly setting baud to ~7500 and
// sending 0x00, which stretches the start bit beyond 13 bit-times at 9600.
// If your converter has a dedicated BREAK pin, drive that instead.
void Alde3020Component::send_lin_break_() {
  // Flush any pending bytes
  this->flush();
  // Write a 0x00 at the normal baud – most LIN TTL bridges accept this
  // as a valid break when sent with a framing error.
  // For a software-break: toggle the TX line low for 1ms via GPIO if needed.
  // Here we rely on the hardware UART generating the break automatically
  // (works on ESP32 with Serial.sendBreak() or via lin_break GPIO).
  //
  // Simple approach: write 0x00 – this gives a long dominant on the bus
  // which acts as a break for practical purposes with most LIN bridges.
  uint8_t brk = 0x00;
  this->write_byte(brk);
  delayMicroseconds(150);  // ensure break is detected
}

// ── Send sync + PID header ─────────────────────────────────────────────────
void Alde3020Component::send_lin_header_(uint8_t lin_id) {
  send_lin_break_();
  this->write_byte(0x55);           // SYNC byte
  this->write_byte(lin_pid_(lin_id)); // PID
}

// ── Encode temperature for control frame (bits 0-5) ───────────────────────
uint8_t Alde3020Component::encode_temp_(float temp_c) {
  if (temp_c < 5.0f)  temp_c = 5.0f;
  if (temp_c > 30.0f) temp_c = 30.0f;
  return (uint8_t)((temp_c - 5.0f) / 0.5f);
}

// ── Decode temperature from info frame ────────────────────────────────────
float Alde3020Component::decode_temp_(uint8_t raw) {
  if (raw <= 0xFA) return (raw * 0.5f) - 42.0f;
  return NAN;  // special values (sensor missing, out of range, invalid)
}

// ── Build and send control frame ──────────────────────────────────────────
void Alde3020Component::send_control_frame_() {
  uint8_t data[8];
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;

  // Byte 3: zone1 temp (bits 0-5) + gas enable (bit 6) + gas valve (bit 7)
  data[3] = encode_temp_(desired_zone1_);
  if (desired_gas_) {
    data[3] |= (1 << 6);  // gas heating enable
    data[3] |= (1 << 7);  // gas valve open
  }

  // Byte 4: zone2 temp (bits 0-5) + electric power (bits 6-7)
  data[4] = encode_temp_(desired_zone2_);
  data[4] |= ((desired_elec_kw_ & 0x03) << 6);

  // Byte 5: system flags
  // Bit 0: panel on/off
  // Bits 3-4: water mode
  // Bit 6: AC auto
  data[5] = 0x00;
  if (desired_panel_)   data[5] |= (1 << 0);
  data[5] |= ((desired_water_mode_ & 0x03) << 3);
  if (desired_ac_auto_) data[5] |= (1 << 6);

  data[6] = 0xFF;
  data[7] = 0xFF;

  uint8_t pid = lin_pid_(LIN_ID_CONTROL);
  uint8_t chk = lin_checksum_(pid, data, 8, !use_classic_checksum_);

  send_lin_header_(LIN_ID_CONTROL);
  for (int i = 0; i < 8; i++) this->write_byte(data[i]);
  this->write_byte(chk);

  ESP_LOGV(TAG, "TX Control: %02X %02X %02X %02X %02X %02X %02X %02X [chk=%02X %s]",
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], chk,
           use_classic_checksum_ ? "classic" : "enhanced");
}

// ── Request info frame (send header only, heater responds) ────────────────
void Alde3020Component::request_info_frame_() {
  // Drain stale bytes before requesting status.
  while (this->available()) this->read();
  send_lin_header_(LIN_ID_INFO);

  // Some LIN/UART bridges echo the sent header (BREAK, SYNC, PID) back to RX.
  // Capture a larger window and later search for a valid 8-byte payload +
  // checksum sequence instead of assuming the first 9 bytes are the frame.
  rx_pos_      = 0;
  rx_active_   = true;
  rx_start_ms_ = millis();
}

// ── Parse 8-byte info frame payload ───────────────────────────────────────
void Alde3020Component::parse_info_frame_(const uint8_t *d) {
  ESP_LOGV(TAG, "RX Info:  %02X %02X %02X %02X %02X %02X %02X %02X",
           d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

  zone1_actual_   = decode_temp_(d[0]);
  zone2_actual_   = decode_temp_(d[1]);
  outdoor_actual_ = decode_temp_(d[2]);

  // Byte 3: zone1 setpoint (bits 0-5) + gas flags
  uint8_t sp1_raw = d[3] & 0x3F;
  if (sp1_raw <= 0x32) {
    zone1_setpoint_ = (sp1_raw * 0.5f) + 5.0f;
  }
  gas_active_     = (d[3] >> 6) & 1;
  gas_valve_open_ = (d[3] >> 7) & 1;

  // Byte 4: zone2 setpoint (bits 0-5) + electric power (bits 6-7)
  uint8_t sp2_raw = d[4] & 0x3F;
  if (sp2_raw <= 0x32) {
    zone2_setpoint_ = (sp2_raw * 0.5f) + 5.0f;
  }
  electric_kw_ = (d[4] >> 6) & 0x03;

  // Byte 5: system status
  panel_on_       = (d[5] >> 0) & 1;
  panel_busy_     = (d[5] >> 1) & 1;
  error_present_  = (d[5] >> 2) & 1;
  water_mode_     = (d[5] >> 3) & 0x03;
  ac_available_   = (d[5] >> 5) & 1;
  ac_auto_        = (d[5] >> 6) & 1;
  pump_running_   = (d[5] >> 7) & 1;

  ESP_LOGD(TAG,
           "Zone1=%.1f°C(set %.1f°C) Zone2=%.1f°C Outdoor=%.1f°C "
           "Gas=%s/%s Elec=%ukW Panel=%s PumpRunning=%s WaterMode=%u Error=%s",
           zone1_actual_, zone1_setpoint_, zone2_actual_, outdoor_actual_,
           gas_active_ ? "ON" : "off", gas_valve_open_ ? "OPEN" : "closed",
           electric_kw_, panel_on_ ? "ON" : "off",
           pump_running_ ? "YES" : "no", water_mode_,
           error_present_ ? "YES" : "no");

  // Notify sub-components
  for (auto &cb : on_state_callbacks_) cb();
}

// ── Component lifecycle ────────────────────────────────────────────────────
void Alde3020Component::setup() {
  ESP_LOGCONFIG(TAG, "Alde 3020 LIN component starting...");
}

void Alde3020Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Alde 3020 Heater:");
  LOG_PIN("  TX Pin: ", nullptr);  // UART handles pin logging
  ESP_LOGCONFIG(TAG, "  Send interval: %u ms", SEND_INTERVAL_MS);
}

void Alde3020Component::loop() {
  // ── Receive bytes from info frame response ──────────────────────────
  if (rx_active_) {
    const uint8_t pid = lin_pid_(LIN_ID_INFO);

    while (this->available() && rx_pos_ < RX_BUF_SIZE) {
      rx_buf_[rx_pos_++] = this->read();
    }

    // Search for a valid 9-byte info frame anywhere in the capture window.
    bool frame_found = false;
    bool frame_classic = false;
    uint8_t frame_offset = 0;

    if (rx_pos_ >= 9) {
      for (uint8_t off = 0; off <= rx_pos_ - 9; off++) {
        const uint8_t *candidate = &rx_buf_[off];
        uint8_t expected_enhanced = lin_checksum_(pid, candidate, 8, true);
        uint8_t expected_classic  = lin_checksum_(pid, candidate, 8, false);
        bool matched_enhanced = (candidate[8] == expected_enhanced);
        bool matched_classic  = (candidate[8] == expected_classic);

        if (matched_enhanced || matched_classic) {
          frame_found = true;
          frame_classic = matched_classic && !matched_enhanced;
          frame_offset = off;
          break;
        }
      }
    }

    if (frame_found) {
      if (frame_classic != use_classic_checksum_) {
        use_classic_checksum_ = frame_classic;
        ESP_LOGI(TAG, "Detected %s LIN checksum mode",
                 use_classic_checksum_ ? "classic" : "enhanced");
      }

      parse_info_frame_(&rx_buf_[frame_offset]);
      rx_active_ = false;
      rx_pos_ = 0;
      while (this->available()) this->read();
    }

    // Timeout
    if (rx_active_ && (millis() - rx_start_ms_ > RX_TIMEOUT_MS)) {
      ESP_LOGW(TAG, "Info frame RX timeout/checksum (got %u bytes, no valid frame)", rx_pos_);

      if (rx_pos_ > 0) {
        char hex_dump[RX_BUF_SIZE * 3 + 1];
        uint16_t p = 0;
        for (uint8_t i = 0; i < rx_pos_ && p + 4 < sizeof(hex_dump); i++) {
          p += snprintf(&hex_dump[p], sizeof(hex_dump) - p, "%02X ", rx_buf_[i]);
        }
        hex_dump[p] = '\0';
        ESP_LOGV(TAG, "RX raw bytes: %s", hex_dump);
      }

      rx_active_ = false;
      rx_pos_    = 0;
      while (this->available()) this->read();
    }
  }

  // ── Send control + request info every SEND_INTERVAL_MS ─────────────
  if (!rx_active_) {
    uint32_t now = millis();
    if (now - last_send_ms_ >= SEND_INTERVAL_MS) {
      last_send_ms_ = now;
      send_control_frame_();
      delayMicroseconds(3000);   // gap between control write and info poll
      request_info_frame_();
    }
  }
}

// ── Setters ───────────────────────────────────────────────────────────────
void Alde3020Component::set_zone1_target(float t)   { desired_zone1_    = t; }
void Alde3020Component::set_zone2_target(float t)   { desired_zone2_    = t; }
void Alde3020Component::set_gas_enabled(bool on)    { desired_gas_      = on; }
void Alde3020Component::set_electric_power(uint8_t kw) { desired_elec_kw_ = kw; }
void Alde3020Component::set_panel_on(bool on)       { desired_panel_    = on; }
void Alde3020Component::set_water_mode(uint8_t m)   { desired_water_mode_ = m; }
void Alde3020Component::set_ac_auto(bool on)        { desired_ac_auto_  = on; }

}  // namespace alde3020
}  // namespace esphome
