# Technical handoff

## Effective baseline

The E branch uses ESP32-S31-WROOM-1-N16R16V. The T branch uses ESP32-P4-Core (16MB SKU36096) plus S31. P4 is display/vision; S31 owns wireless/audio/tasks/peripherals. Do not add a redundant C6 wireless module or revive the historical Linux/BCM route by default. SDK is pinned; S31 hardware availability and SDK preview status are separate facts.

## What exists

- E6 driver validates the six-color frame, bounds BUSY waits, retains the vendor command sequence and records uncertainty after failures. It does not prove optical refresh or low whole-device power.
- HFP HF external-codec HCI builds in Classic and Classic/BLE configurations. The dual-mode patch retains BLE controller memory and enables BTDM. No ANCS/AMS/GATT application or phone/Wi-Fi concurrency is validated.
- The journal commits intent before exposing dispatch tokens, converts restored RUNNING to UNKNOWN instead of replaying, persists cancellation/receipts, and retains per-ID records. CRC is corruption detection, not signature verification. Trusted transport, archival/rotation, global multi-task scheduling, and real Flash power-cut tests remain open.
- ES8388/I2S uses MCLK16/BCLK12/WS13/TX14/RX15, I2C SCL6/SDA7. The codec component uses 8-bit 0x20 and internally shifts to 7-bit 0x10. Duplex PCM is 16kHz/16-bit/stereo. NS4150 GPIO17 stays low and DAC stays muted by default; probe is compile-disabled unless explicitly enabled.
- Current power allocation uses GPIO8 EN1, GPIO11 EN2, GPIO9 CC IRQ; these are not spare contact-PDM inputs. IR resources are GPIO42/43/44. Do not reintroduce old BCM pin numbers.

## First useful next work

Connect actual HFP CVSD/mSBC encoding/decoding and resampling to the PCM interface, then implement directional recording/storage and task receipts; or finish a concrete peripheral schematic with verified pin/electrical evidence. Keep these as bounded work units. Do not repeatedly compile the stock loopback example and call that phone audio integration.

The I2S analog Codec does not decode raw SCO payloads. RIFF headers are format helpers, not a storage writer or duration proof. Preallocated OTA slots are not an implemented update/rollback workflow. The projects remain independent images; integrated firmware and hardware are not accepted.

For product constraints, private review decisions and original conversations, ask the owner to provide the private handoff archive in an authorized workspace. That archive is deliberately not published here.
