#include "alde3030.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>
#include <cstdio>
#ifdef USE_ESP32
#include "esphome/components/uart/uart_component_esp_idf.h"
#endif

namespace esphome {
namespace alde3030 {

// ── LIN PID: add parity bits (P0, P1) per LIN spec ────────────────────────
uint8_t Alde3030Component::lin_pid_(uint8_t id) {
  uint8_t p0 = ((id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 1;
  uint8_t p1 = (~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5))) & 1;
  return (id & 0x3F) | (p0 << 6) | (p1 << 7);
}

// ── LIN checksum ───────────────────────────────────────────────────────────
uint8_t Alde3030Component::lin_checksum_(uint8_t pid, const uint8_t *data, size_t len, bool include_pid) {
  uint16_t sum = include_pid ? pid : 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
    if (sum > 0xFF) sum -= 0xFF;
  }
  return (uint8_t)(~sum & 0xFF);
}

// ── Send a LIN break: hold TX low for >13 bit-times ──────────────────────
void Alde3030Component::send_lin_break_() {
  this->flush();
#ifdef USE_ESP32
  // get_hw_serial_number() returns uint8_t; cast to uart_port_t explicitly.
  uart_port_t uart_num = static_cast<uart_port_t>(
      static_cast<uart::IDFUARTComponent *>(this->parent_)->get_hw_serial_number());
  uart_wait_tx_done(uart_num, pdMS_TO_TICKS(100));
  // uart_set_break() is only available in ESP-IDF ≥5.0.  Use a portable
  // baud-rate-swap instead: at 4800 baud a single 0x00 byte holds TX
  // dominant for ~2.1 ms, which is ≈40 bit-times at 19200 baud (spec
  // requires >13), then restore the original rate.
  uart_set_baudrate(uart_num, 4800);
  this->write_byte(0x00);
  uart_wait_tx_done(uart_num, pdMS_TO_TICKS(20));
  uart_set_baudrate(uart_num, 19200);
#else
  this->write_byte(0x00);
  delayMicroseconds(1000);
#endif
}

// ── Send sync + PID header ─────────────────────────────────────────────────
void Alde3030Component::send_lin_header_(uint8_t lin_id) {
  send_lin_break_();
  this->write_byte(0x55);
  this->write_byte(lin_pid_(lin_id));
}

// ── Encode setpoint for control frame (bits 0-5 of byte 3) ───────────────
uint8_t Alde3030Component::encode_setpoint_(float temp_c) {
  if (temp_c < 5.0f)  temp_c = 5.0f;
  if (temp_c > 30.0f) temp_c = 30.0f;
  return (uint8_t)((temp_c - 5.0f) / 0.5f);
}

// ── Decode temperature from info frame ────────────────────────────────────
float Alde3030Component::decode_temp_(uint8_t raw) {
  if (raw <= 0xFA) return (raw * 0.5f) - 42.0f;
  return NAN;
}

// ── Send diagnostic/init frame ────────────────────────────────────────────
// The 3030 Plus requires this frame at startup to join the LIN bus correctly.
void Alde3030Component::send_diag_frame_() {
  uint8_t pid = lin_pid_(LIN_ID_DIAG);
  uint8_t chk = lin_checksum_(pid, DIAG_PAYLOAD, 8, true);
  send_lin_header_(LIN_ID_DIAG);
  for (int i = 0; i < 8; i++) this->write_byte(DIAG_PAYLOAD[i]);
  this->write_byte(chk);
  ESP_LOGD(TAG, "TX Diag init frame sent");
}

// ── Build and send control frame ──────────────────────────────────────────
// Read-modify-write approach: bytes 4 and 5 are based on the last values
// reported by the heater so that read-only status bits are echoed unchanged.
// Before the first info frame is received, safe zero-initialised defaults
// are used so the heater is not disturbed.
void Alde3030Component::send_control_frame_() {
  uint8_t data[8];

  // Bytes 0-2: always 0x00 per protocol
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;

  // Byte 3: setpoint [bits 0-5] + gas enable [bit 6] + gas valve echo [bit 7]
  data[3] = encode_setpoint_(desired_zone1_);
  if (desired_gas_) data[3] |= (1 << 6);
  if (gas_valve_open_) data[3] |= (1 << 7);   // echo back heater's valve state

  // Byte 4: preserve low 6 bits from last info frame (may hold zone-2 or
  //         other heater-managed state); overwrite bits 6-7 with electric kW.
  data[4] = (raw_b4_ & 0x3F) | ((desired_elec_kw_ & 0x03) << 6);

  // Byte 5: start from heater's last report to preserve read-only flags:
  //   bit 1 = panel_busy, bit 2 = error, bit 5 = ac_available, bit 7 = pump_running
  // Then apply writeable fields:
  //   bit 0 = panel_on, bits 3-4 = water_mode
  data[5] = raw_b5_;
  data[5] = (data[5] & 0b11100110);              // clear bits 0, 3, 4
  if (desired_panel_) data[5] |= (1 << 0);
  data[5] |= ((desired_water_mode_ & 0x03) << 3);

  // Bytes 6-7: always 0xFF per protocol
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
void Alde3030Component::request_info_frame_() {
  while (this->available()) this->read();
  send_lin_header_(LIN_ID_INFO);
  rx_pos_      = 0;
  rx_active_   = true;
  rx_start_ms_ = millis();
}

// ── Parse 8-byte info frame payload ───────────────────────────────────────
void Alde3030Component::parse_info_frame_(const uint8_t *d) {
  ESP_LOGV(TAG, "RX Info:  %02X %02X %02X %02X %02X %02X %02X %02X",
           d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

  // Byte 0: zone 1 actual temperature
  zone1_actual_   = decode_temp_(d[0]);
  // Byte 1: not exposed on 3030 Plus (may be zone 2 or unused)
  // Byte 2: outdoor temperature
  outdoor_actual_ = decode_temp_(d[2]);

  // Byte 3: zone 1 setpoint [bits 0-5] + gas flags
  uint8_t sp1_raw = d[3] & 0x3F;
  if (sp1_raw <= 0x32) {
    zone1_setpoint_ = (sp1_raw * 0.5f) + 5.0f;
  }
  gas_active_     = (d[3] >> 6) & 1;
  gas_valve_open_ = (d[3] >> 7) & 1;

  // Byte 4: preserve raw value; expose electric kW from bits 6-7
  raw_b4_      = d[4];
  electric_kw_ = (d[4] >> 6) & 0x03;

  // Byte 5: system status — preserve raw for echo, then decode fields
  raw_b5_         = d[5];
  panel_on_       = (d[5] >> 0) & 1;
  panel_busy_     = (d[5] >> 1) & 1;
  error_present_  = (d[5] >> 2) & 1;
  // Bits 3-4: water mode (0=Off 1=On 2=Boost; 3 also maps to On)
  uint8_t wm      = (d[5] >> 3) & 0x03;
  water_mode_     = (wm == 3) ? 1 : wm;  // remap 3→1 (On) to match 3-option select
  ac_available_   = (d[5] >> 5) & 1;
  pump_running_   = (d[5] >> 7) & 1;

  first_info_received_ = true;

  ESP_LOGD(TAG,
           "Zone1=%.1f°C(set %.1f°C) Outdoor=%.1f°C "
           "Gas=%s/%s Elec=%ukW Panel=%s PumpRunning=%s WaterMode=%u Error=%s",
           zone1_actual_, zone1_setpoint_, outdoor_actual_,
           gas_active_ ? "ON" : "off", gas_valve_open_ ? "OPEN" : "closed",
           electric_kw_, panel_on_ ? "ON" : "off",
           pump_running_ ? "YES" : "no", water_mode_,
           error_present_ ? "YES" : "no");

  for (auto &cb : on_state_callbacks_) cb();
}

// ── Component lifecycle ────────────────────────────────────────────────────
void Alde3030Component::setup() {
  ESP_LOGCONFIG(TAG, "Alde 3030 Plus LIN component starting...");
  // Send diagnostic init frame to wake the 3030 Plus bus.
  // A short delay ensures the UART is ready before the first transmission.
  delay(100);
  send_diag_frame_();
  delay(50);
}

void Alde3030Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Alde 3030 Plus Heater:");
  ESP_LOGCONFIG(TAG, "  Send interval: %u ms", SEND_INTERVAL_MS);
}

void Alde3030Component::loop() {
  // ── Receive bytes from info frame response ──────────────────────────
  if (rx_active_) {
    const uint8_t pid = lin_pid_(LIN_ID_INFO);

    while (this->available() && rx_pos_ < RX_BUF_SIZE) {
      rx_buf_[rx_pos_++] = this->read();
    }

    // Search the capture window for a valid 9-byte payload (8 data + checksum).
    bool frame_found   = false;
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
          frame_found   = true;
          frame_classic = matched_classic && !matched_enhanced;
          frame_offset  = off;
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
      rx_pos_    = 0;
      while (this->available()) this->read();
    }

    if (rx_active_ && (millis() - rx_start_ms_ > RX_TIMEOUT_MS)) {
      ESP_LOGW(TAG, "Info frame RX timeout (got %u bytes, no valid frame)", rx_pos_);
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

  // ── Send control + request info every SEND_INTERVAL_MS ──────────────
  if (!rx_active_) {
    uint32_t now = millis();
    if (now - last_send_ms_ >= SEND_INTERVAL_MS) {
      last_send_ms_ = now;
      send_control_frame_();
      delayMicroseconds(3000);
      request_info_frame_();
    }
  }
}

// ── Setters ───────────────────────────────────────────────────────────────
void Alde3030Component::set_zone1_target(float t)      { desired_zone1_      = t; }
void Alde3030Component::set_gas_enabled(bool on)       { desired_gas_        = on; }
void Alde3030Component::set_electric_power(uint8_t kw) { desired_elec_kw_    = kw; }
void Alde3030Component::set_panel_on(bool on)          { desired_panel_      = on; }
void Alde3030Component::set_water_mode(uint8_t m)      { desired_water_mode_ = m; }

}  // namespace alde3030
}  // namespace esphome
