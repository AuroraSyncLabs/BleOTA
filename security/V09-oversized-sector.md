# V09 — Oversized sector acked as stored, then truncated

| | |
|---|---|
| Severity | Low |
| Backend | NimBLE, plain OTA |
| Location | `ble_ota/src/nimble_ota.c`: `BUF_LENGTH`, sector delivery in `esp_ble_ota_write_chr()` |
| Tests | `ble_ota/test/test_ble_ota.c`: "sector larger than 4096 bytes is refused" |
| Commits | `cda3b15` (test), `0974685` (fix) |

## Problem

The sector buffer kept upstream's 4098-byte size (upstream stored the 2 CRC bytes in it; this fork
strips them). A sector of 4097 or 4098 bytes was accepted, its CRC checked over all of its bytes,
then given to the application as `fw_buf_offset < 4098 ? fw_buf_offset : 4096`:

- 4098 bytes: only 4096 reached the application, yet the sector was acked "stored";
- 4097 bytes: the application received a sector longer than the 4096 bytes the protocol promises.

## Impact

Firmware bytes are lost silently while the peer is told they were stored. The image is then
corrupt; it fails later in `esp_ota_end()` (hash or signature), or worse in an application that
skips that check. An application that sizes its buffers to 4096 bytes gets one more byte.

## Before

```
test/test_ble_ota.c:329:sector larger than 4096 bytes is refused:FAIL: Expected 0 to be not equal to 0. Function [ble_ota][security]
```

The 4098-byte sector was acked "stored" and the application received 4096 bytes.

## Fix

- `SECTOR_SIZE` (4096) replaces `BUF_LENGTH` (4098): a sector longer than 4096 bytes takes the
  existing overflow path and is never acked.
- The application receives exactly the bytes covered by the sector CRC.
- `PROTOCOL.md` says that a longer sector is rejected. The plain-mode overflow log no longer talks
  about ciphertext.

## After

```
E (286) NimBLE_BLE_OTA: esp_ble_ota_write_chr: sector longer than 4096 bytes
E (286) NimBLE_BLE_OTA: ble_ota_gatt_handler: firmware write failed: ESP_ERR_INVALID_SIZE
test/test_ble_ota.c:319:sector larger than 4096 bytes is refused:PASS
```

A full 4096-byte sector is still stored (same test). All 17 NimBLE cases pass. The pre-encrypted
configuration builds.
