# V06 — Packet header read from the first mbuf only

| | |
|---|---|
| Severity | Medium |
| Backend | NimBLE (plain and pre-encrypted OTA) |
| Location | `ble_ota/src/nimble_ota.c`: `esp_ble_ota_write_chr()`, sector ack helpers |
| Tests | `ble_ota/test/test_ble_ota.c`: "packet header is read from the whole mbuf chain" |
| Commits | `67f13fc` (test), `2fe2c46` (fix) |

## Problem

A write reaches the access callback as an mbuf chain. The length check used `os_mbuf_len()` (whole
chain) and the payload was copied with `os_mbuf_copydata()`, but the 3-byte header (sector index,
packet index) and the bytes echoed in every sector ack were read straight from `om->om_data[0..2]`,
the first mbuf only. NimBLE can leave an emptied mbuf at the head of the chain (`os_mbuf_adj()` after
stripping the ATT header, or reassembled ACL fragments), so those bytes are stale buffer contents.

## Impact

- Valid packets get rejected or stored under the wrong index, depending on stale memory.
- The two stale bytes are sent back to the peer in the sector ack: a small memory disclosure.

This is the same class of bug as upstream fix #502, which covered the payload but not the header.

## Before

```
E (292) NimBLE_BLE_OTA: esp_ble_ota_write_chr - sector index error, cur: 0, recv: 43690
E (292) NimBLE_BLE_OTA: ble_ota_gatt_handler: firmware write failed: ESP_ERR_INVALID_STATE
test/test_ble_ota.c:283:packet header is read from the whole mbuf chain:FAIL: Expected 0 Was 14. Function [ble_ota][security]
```

The stale `0xaa 0xaa` bytes became sector 43690, and the resend ack echoed them.

## Fix

The header is copied from the chain with `os_mbuf_copydata()` (the length check above guarantees 3
bytes). The resend, abort and stored acks are built from the parsed sector index, not from
`om_data`.

## After

```
test/test_ble_ota.c:268:packet header is read from the whole mbuf chain:PASS
```

All 15 NimBLE cases pass. The pre-encrypted configuration builds.
