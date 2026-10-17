# V05 — Announced firmware length not enforced

| | |
|---|---|
| Severity | Medium |
| Backend | NimBLE, plain OTA (`CONFIG_OTA_WITHOUT_SECURITY`) |
| Location | `ble_ota/src/nimble_ota.c`: `esp_ble_ota_write_chr()`, STOP in `ble_ota_start_write_chr()` |
| Tests | `ble_ota/test/test_ble_ota.c`, tag `[security]` |
| Commits | `d8da6e4` (tests), `1840459` (fix) |

## Problem

START announces the firmware length (bytes 2–5). The component stored it but never compared it
with what it received:

- a STOP with no transfer running (never started, refused by the start callback, dropped) still
  called the end callback, or was acked "accepted" without one;
- a STOP after part of the image did the same;
- sectors past the announced length were forwarded to the sector callback.

## Impact

A peer can make the application finish an update that never happened or is truncated. An
application that calls `esp_ota_end()` / `esp_ota_set_boot_partition()` from the end callback, or
sizes its storage from the start callback, relies on the component for these checks. Without an end
callback the peer is told "accepted" for an incomplete image. Pre-encrypted OTA already refused
these STOPs through `esp_encrypted_img_decrypt_end()`.

## Before

```
test/test_ble_ota.c:303:STOP without a transfer is refused:FAIL: Expected 0 Was 1. Function [ble_ota][security]
test/test_ble_ota.c:313:STOP before the whole firmware is received is refused:FAIL: Expected 0 Was 1. Function [ble_ota][security]
test/test_ble_ota.c:321:firmware beyond the announced length aborts the transfer:FAIL: Expected 0 to be not equal to 0. Function [ble_ota][security]
```

The end callback ran for both STOPs, and the extra sector was stored and acked.

## Fix

- Count the bytes given to the application (`ota_recv_len`), reset with the rest of the transfer
  state.
- A sector that would go past the announced length aborts the transfer: sector ack `0x0003`, abort
  callback, later STOP refused. Plain and pre-encrypted OTA share `ble_ota_abort_transfer()`.
- STOP is refused (ack `0x0001`, no end callback) unless a transfer is running and all announced
  bytes arrived. The abort callback runs when a transfer was dropped this way.
- `PROTOCOL.md` and the abort callback documentation describe the new cases.

## After

```
E (294) NimBLE_BLE_OTA: STOP after 0 of 0 bytes
test/test_ble_ota.c:299:STOP without a transfer is refused:PASS
E (294) NimBLE_BLE_OTA: STOP after 12 of 24 bytes
test/test_ble_ota.c:307:STOP before the whole firmware is received is refused:PASS
E (294) NimBLE_BLE_OTA: esp_ble_ota_write_chr: sector 0 beyond the announced length, aborting
test/test_ble_ota.c:318:firmware beyond the announced length aborts the transfer:PASS
```

All 14 NimBLE cases pass. The existing STOP cases now send the whole image first.
