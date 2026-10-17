# V08 — START during a transfer skips the abort callback

| | |
|---|---|
| Severity | Low |
| Backend | NimBLE (plain and pre-encrypted OTA) |
| Location | `ble_ota/src/nimble_ota.c`: START in `ble_ota_start_write_chr()` |
| Tests | `ble_ota/test/test_ble_ota.c`: "START during a transfer aborts the previous one" |
| Commits | `89b93d5` (test), `b902ed0` (fix), `3a43705` (documentation) |

## Problem

A START received while a transfer runs resets the component's state and calls the start callback
again. The application is never told that the first transfer was dropped: the abort callback only
ran on disconnect.

## Impact

Whatever the application opened in its start callback (an `esp_ota_begin()` handle, a buffer, a
task message) for the first transfer is left behind. A peer that keeps sending START can leak those
resources until the application runs out of memory or OTA handles, unless the application guards
against it itself. Under pre-encrypted OTA the component already released its own decrypt handle,
but not the application's state.

## Before

```
test/test_ble_ota.c:211:START during a transfer aborts the previous one:FAIL: Expected 1 Was 0. Function [ble_ota][security]
```

## Fix

A START that finds a transfer running first drops it through `ble_ota_abort_transfer()` (the same
path as the other aborts), so the abort callback runs before the new start callback. The abort
callback documentation lists this case.

## After

```
test/test_ble_ota.c:207:START during a transfer aborts the previous one:PASS
```

All 16 NimBLE cases pass. The pre-encrypted configuration builds.
