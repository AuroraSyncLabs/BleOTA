# AuroraSync BLE OTA Component

ESP-IDF component to update firmware over BLE. Fork of Espressif's
[`ble_ota`](https://github.com/espressif/esp-iot-solution/tree/master/components/bluetooth/ble_profiles/esp/ble_ota),
published on the [ESP Component Registry](https://components.espressif.com/components/aurorasynclabs/ble_ota).

## Changes from upstream

- Runs on an application-owned NimBLE host (`CONFIG_BLE_OTA_EXTERNAL_HOST`), e.g. alongside Matter.
- Start, sector and end callbacks: the application can refuse a transfer, ask for a sector again and
  report whether the image was accepted.
- Encrypted link required on every characteristic, acks sent as notifications.
- ESP32-S31 support.

## Quick start

```bash
idf.py add-dependency aurorasynclabs/ble_ota
```

To run on your own NimBLE host, set in `sdkconfig.defaults`:

```
CONFIG_BT_NIMBLE_DYNAMIC_SERVICE=y
CONFIG_BLE_OTA_EXTERNAL_HOST=y
```

| Documentation   | Where                                                    |
|-----------------|----------------------------------------------------------|
| BLE protocol    | [`ble_ota/PROTOCOL.md`](ble_ota/PROTOCOL.md)             |
| API reference   | [`ble_ota/include/ble_ota.h`](ble_ota/include/ble_ota.h) |
| Minimal example | [`ble_ota/test_app/`](ble_ota/test_app/)                 |
| Changelog       | [`ble_ota/CHANGELOG.md`](ble_ota/CHANGELOG.md)           |

## Maintainer

<div align="center">

| [<img src="https://github.com/AnabolicRoo.png" width="85"><br>**Julien ROIRON**](https://github.com/AnabolicRoo) |
|:----------------------------------------------------------------------------------------------------------------:|

</div>
