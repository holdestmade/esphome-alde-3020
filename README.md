# Alde 3020 ESPHome Component

An ESPHome external component for controlling and monitoring an Alde 3020 heater
via LIN bus using an ESP32 and a LIN-to-TTL converter.

## Hardware Required

| Component | Notes |
|---|---|
| ESP32 (any variant) | ESP32-WROOM-32 recommended |
| LIN-to-TTL module | e.g. MCP2025, TJA1020, or generic LIN transceiver breakout |
| 12V supply | From caravan 12V bus |

## Wiring

```
Caravan 12V ──────────────────────────────────┐
                                              │
                                     ┌────────┴────────┐
ESP32 GPIO17 (TX2) ─────────────── TX│                 │LIN ──── Alde 3020 LIN wire
ESP32 GPIO16 (RX2) ─────────────── RX│  LIN-TTL Module │
GND ───────────────────────────── GND│                 │
                                     └────────┬────────┘
                                              │
Caravan GND ──────────────────────────────────┘
```

The Alde 3020 LIN wire is typically found on the heater control connector.
Consult your heater's wiring diagram — LIN is a single-wire half-duplex bus
running at 9600 baud, 12V dominant/recessive logic.

> **Important**: The ESP32 TX/RX are 3.3V. Never connect them directly to the
> LIN bus — always use the LIN transceiver module to level-shift to 12V.

## Home Assistant Entities Created

| Entity | Type | Description |
|---|---|---|
| Alde Zone 1 | Climate | Thermostat for zone 1 (current + target temp, on/off) |
| Alde Zone 2 | Climate | Thermostat for zone 2 (if installed) |
| Alde Zone 1 Temperature | Sensor | Actual measured temperature |
| Alde Zone 2 Temperature | Sensor | Actual measured temperature |
| Alde Outdoor Temperature | Sensor | External sensor reading |
| Alde Zone 1/2 Target | Sensor | Current setpoint reported by heater |
| Alde Electric Power | Sensor | Active electric heating in kW |
| Alde Panel | Switch | Main heater panel on/off |
| Alde Gas Heating | Switch | Enable/disable gas burner |
| Alde AC Auto Mode | Switch | Automatic AC power management |
| Alde Electric Power Level | Select | Off / 1kW / 2kW / 3kW |
| Alde Water Heating Mode | Select | Off / Normal / Boost / Auto |
| Alde Pump Running | Binary Sensor | Circulation pump state |
| Alde Error | Binary Sensor | Fault indicator |
| Alde Gas Valve Open | Binary Sensor | Gas valve state |
| Alde AC Power Available | Binary Sensor | 230V input present |

## LIN Break Signal Notes

Generating a proper LIN break (dominant bus for >13 bit-times) from software
varies by hardware. This component uses the simplest approach — writing a 0x00
byte — which most LIN-TTL bridge modules interpret correctly as a break condition.

If your module has a dedicated BREAK or INH pin, you can alternatively:

1. Pull the INH pin low for ~1.5ms before transmitting.
2. Or use `esphome`'s GPIO output to toggle a break GPIO:

```yaml
output:
  - platform: gpio
    id: lin_break_pin
    pin: GPIO4   # connect to your module's INH/BREAK pin
```

Then in a custom lambda, pulse it before each frame. The component's
`send_lin_break_()` method can be overridden for this purpose.

## Temperature Ranges

| Parameter | Range | Resolution |
|---|---|---|
| Zone setpoint (control) | 5°C – 30°C | 0.5°C steps |
| Zone actual (info) | -42°C – 83°C | 0.5°C steps |
| Outdoor temp (info) | -42°C – 83°C | 0.5°C steps |

## Troubleshooting

**No data received from heater:**
- Check LIN bus wiring and that the module has 12V power.
- Enable `logger: level: VERBOSE` and look for `TX Control:` and `RX Info:` log lines.
- Verify baud rate is 9600 with no parity.
- Confirm the LIN module's TX and RX are not swapped.

**Checksum errors:**
- The component uses the LIN 2.x *enhanced* checksum (includes PID byte).
  If your heater uses LIN 1.x *classic* checksum (excludes PID), change
  `lin_checksum_` to start `uint16_t sum = 0;` instead of `uint16_t sum = pid;`.

**Temperatures show NaN / unavailable:**
- Special values (0xFB–0xFF) from the info frame mean sensor missing or out of
  range. Check sensor connections on the heater itself.

**Zone 2 always shows "unused":**
- Single-zone Alde units report 0xFE for Zone 2 — this is normal. Remove the
  zone 2 climate/sensor entities from your YAML.

## Protocol Reference

This component implements the Alde LIN protocol as documented:
- **Control frame ID**: 0x1A (master sends to heater)
- **Info frame ID**: 0x1B (master requests, heater responds)
- **Baud rate**: 9600
- **Frame length**: 8 bytes + checksum
- **Checksum**: LIN 2.x enhanced (includes PID)

## Tested With

- Alde Compact 3020 (single zone, Function ID 0x0001)
- ESPHome 2024.x
- ESP32-WROOM-32
- Generic MCP2025-based LIN transceiver breakout

## License

MIT — use freely, no warranty implied.
