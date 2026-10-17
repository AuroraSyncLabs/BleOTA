## v1.0.0 - 2026.10.17

Breaking changes:
- NimBLE only: the Bluedroid and protocomm backends are removed, with `esp_ble_ota_write()`,
  `esp_ble_ota_set_sizes()`, `esp_ble_ota_finish()`, `CONFIG_OTA_WITH_PROTOCOMM` and the
  `espressif/cjson` dependency.

Bugfix:
- Plain OTA enforces the announced length: firmware beyond it aborts the transfer, a STOP without a
  running transfer or before the whole firmware is refused (V05).
- Read the packet header from the whole mbuf chain (V06).
- A START during a transfer aborts it and calls the abort callback (V08).
- Refuse sectors larger than 4096 bytes instead of acking them and truncating them (V09).

## v0.5.1 - 2026.10.17

Bugfix:
- Unit tests: define `crc16_ccitt` before its first use.

## v0.5.0 - 2026.10.17

Breaking changes:
- The sector CRC16 is checked: a wrong CRC (e.g. a lost packet) makes the device ask for the sector
  again.

Features:
- Unity tests run on QEMU, minimal example in `examples/minimal`.

Bugfix:
- Reject commands shorter than 20 bytes with the ATT error `0x0D`.
- Remove a debug `printf` on STOP.

## v0.4.0 - 2026.10.15

Features:
- `esp_ble_ota_gatt_svcs()` returns the OTA service table, to register before the GATT server
  starts. `CONFIG_BLE_OTA_EXTERNAL_HOST` no longer requires `CONFIG_BT_NIMBLE_DYNAMIC_SERVICE`, only
  `esp_ble_ota_gatt_register()` does.

## v0.3.0 - 2026.10.15

Breaking changes:
- Acks are notifications instead of indications.
- Every characteristic requires an encrypted link.
- Pre-encrypted OTA: the component releases the decrypt handle on abort and disconnect, the
  application must no longer call `esp_encrypted_img_decrypt_abort()`.

Features:
- `CONFIG_BLE_OTA_EXTERNAL_HOST` runs the profile on an application-owned NimBLE host, through
  `esp_ble_ota_gatt_register()`, `esp_ble_ota_on_connect()`, `esp_ble_ota_on_disconnect()` and
  `esp_ble_ota_on_subscribe()`.
- Start, sector, end and abort callbacks. `esp_ble_ota_end_ack()` sends the STOP ack once the
  application has verified the image.
- Pre-encrypted OTA: `esp_ble_ota_set_decrypt_cfg()` opens a decrypt handle on every START. A STOP
  is refused when the image is incomplete or not authentic, a sector that cannot be decrypted or
  stored aborts the transfer (sector ack `0x0003`).
- Write without response on RECV_FW.
- `PROTOCOL.md` describes the BLE protocol.

Bugfix:
- A disconnect resets the transfer state.
- Pre-encrypted OTA: refuse a START announcing 512 bytes or less.

## v0.1.19 - 2026.10.14

Features:
- Fork of `espressif/ble_ota` v0.1.18, published as `aurorasynclabs/ble_ota`.
- Add support for ESP32-S31.

## v0.1.18 - 2026.09.09

Bugfix:
- BLE-OTA: Limit Bluedroid RECV_FW characteristic to `ESP_GATT_MAX_ATTR_LEN`

## v0.1.17 - 2026.08.25

Bugfix:
- BLE-OTA: Use espressif/cjson on ESP-IDF v6.0 and later; keep the built-in json component on older IDF versions.

## v0.1.16 - 2026.04.15

Bugfix:
- BLE-OTA: Fixed CONFIG_PRE_ENC_OTA on NimBLE to decrypt after sector/packet checks, preventing AES-GCM breakage when packets were lost or sectors retried.

## v0.1.15 - 2025.07.29

Bugfix:
- BLE-OTA: Fixed BLE OTA data corruption issue caused by incomplete NimBLE's mbufs reading [#502](https://github.com/espressif/esp-iot-solution/pull/502)

## v0.1.14 - 2024.11.21

Features:
- BLE-OTA: Add support for enabling BLE 5.0 for NimBLE

## v0.1.13 - 2024.10.21

Features:
- BLE-OTA: Add support for Delta OTA.

## v0.1.12 - 2024.02.19

Features:
- BLE-OTA: Add support for ESP32C2 and ESP32C6.

## v0.1.11 - 2024.01.02

Bugfix:
- BLE-OTA: Fixed ble ota packet index issue.

## v0.1.10 - 2023.11.23

Bugfix:
- Fix possible cmake_utilities dependency issue

## v0.1.9 - 2023.10.16

Features:
- BLE-OTA: Add support for ESP32H2

## v0.1.8 - 2023.03.31

Bugfix:
- BLE-OTA: Fixed errors for release v4.4

## v0.1.7 - 2023.03.09

Features:
- BLE-OTA: Use cu_pkg_define_version to define the version of this component.

## v0.1.6 - 2023.02.17

Features:
- BLE-OTA: Added support for nimble extended advertising.

## v0.1.5 - 2023.02.15

Features:
- BLE-OTA: Added support for pre-encrypted OTA.

## v0.1.4 - 2023.02.13

Features:
- BLE-OTA: Added support for bluedroid BLE 5.0 features.

## v0.1.3 - 2023.02.09

Features:
- BLE-OTA: Added support for IDF release v4.4

Bugfix:
- BLE-OTA: Fixed github ble_ota link in idf_component.yml

## v0.1.2 - 2023.01.18

Bugfix:
- Fixed warnings

## v0.1.1 - 2023.01.17

Features:
- BLE-OTA: Deleted subscribe command support, no longer needed for protocomm.
- BLE-OTA: Added finish command support to indicated end of OTA.

## v0.1.0 - 2023.01.04

Features:
- BLE-OTA: The initial version.
