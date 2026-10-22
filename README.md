# AuroraSync BLE OTA Component

[![CI](https://github.com/AuroraSyncLabs/BleOTA/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/AuroraSyncLabs/BleOTA/actions/workflows/ci.yml)

ESP-IDF component to update firmware over BLE. Fork of Espressif's
[`ble_ota`](https://github.com/espressif/esp-iot-solution/tree/master/components/bluetooth/ble_profiles/esp/ble_ota),
published on the [ESP Component Registry](https://components.espressif.com/components/aurorasynclabs/ble_ota).

Maintained as-is, for AuroraSync's own firmware: issues and pull requests are read, with no promised reply time.
Licensed under [Apache-2.0](LICENSE), like upstream.

## Changes from upstream

- NimBLE only: the Bluedroid and protocomm backends are removed. With Bluedroid, use upstream
  `espressif/ble_ota`.
- Runs on an application-owned NimBLE host (`CONFIG_BLE_OTA_EXTERNAL_HOST`), e.g. alongside Matter.
- Start, sector and end callbacks: the application can refuse a transfer, ask for a sector again and
  report whether the image was accepted.
- Encrypted link required on every characteristic, acks sent as notifications.
- Sector CRC checked: a lost packet makes the device ask for the sector again.
- Announced length enforced: extra firmware aborts the transfer, an early STOP is refused.
- ESP32-S31 support.

## Quick start

```bash
idf.py add-dependency aurorasynclabs/ble_ota
```

To run on your own NimBLE host, set in `sdkconfig.defaults`:

```
CONFIG_BLE_OTA_EXTERNAL_HOST=y
```

Then register the table from `esp_ble_ota_gatt_svcs()` with your own services before the GATT server
starts. `esp_ble_ota_gatt_register()` adds it later instead, but needs
`CONFIG_BT_NIMBLE_DYNAMIC_SERVICE=y` and can exhaust the NimBLE client configuration pool (see
`ble_ota.h`).

| Documentation   | Where                                                    |
|-----------------|----------------------------------------------------------|
| BLE protocol    | [`ble_ota/PROTOCOL.md`](ble_ota/PROTOCOL.md)             |
| API reference   | [`ble_ota/include/ble_ota.h`](ble_ota/include/ble_ota.h) |
| Minimal example | [`ble_ota/examples/minimal/`](ble_ota/examples/minimal/) |
| Changelog       | [`ble_ota/CHANGELOG.md`](ble_ota/CHANGELOG.md)           |
| Security audit  | [`security/`](security/)                                 |

## Tests

CI runs on every pull request: formatting, `idf.py clang-check`, unit tests on QEMU and the example
builds. Unit tests live in `ble_ota/test/` and run through `ble_ota/test_apps/` with
[pytest](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/esp-idf-tests-with-pytest.html)
(`install.sh --enable-ci`, plus `idf_tools.py install qemu-riscv32` for QEMU). From `ble_ota/`:

```shell
# Formatting, pinned version through uv (https://docs.astral.sh/uv/)
git ls-files '*.c' '*.h' | xargs uvx clang-format==23.1.1 --style=file -i
# QEMU, as in CI
idf-ci build run -p test_apps -t esp32c3 -m qemu && pytest -m qemu --target esp32c3
```

## Maintainer

<div align="center">

| [<img src="https://github.com/AnabolicRoo.png" width="85"><br>**Julien ROIRON**](https://github.com/AnabolicRoo) |
|:----------------------------------------------------------------------------------------------------------------:|

</div>
