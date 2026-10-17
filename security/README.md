# Security audit

Audit of the component as of tag `pre-cleanup`. Each fixed finding has a regression test (tag
`[security]` in `ble_ota/test/test_ble_ota.c`) and a before/after report.

| ID  | Finding                                                    | Severity | Status                                        |
|-----|------------------------------------------------------------|----------|-----------------------------------------------|
| V01 | Bluedroid sector buffer: no size or minimum-length checks  | Critical | Removed with the Bluedroid backend (`3bd717e`) |
| V02 | Bluedroid prepared-write buffer: no bounds, never reset    | High     | Removed with the Bluedroid backend (`3bd717e`) |
| V03 | Protocomm payload type not checked against the message     | High     | Removed with the protocomm backend (`78ddb81`) |
| V04 | Bluedroid characteristics do not require encryption        | High     | Removed with the Bluedroid backend (`3bd717e`) |
| V05 | [Announced firmware length not enforced](V05-announced-length.md) | Medium | Fixed (`1840459`)                       |
| V06 | [Packet header read from the first mbuf only](V06-mbuf-header.md) | Medium | Fixed (`2fe2c46`)                       |
| V07 | Bluedroid disconnect does not reset the transfer           | Medium   | Removed with the Bluedroid backend (`3bd717e`) |
| V08 | [START during a transfer skips the abort callback](V08-superseding-start.md) | Low | Fixed (`b902ed0`)            |
| V09 | [Oversized sector acked, then truncated](V09-oversized-sector.md) | Low | Fixed (`0974685`)                         |

## Not fixed: design notes

- **Peer authentication.** The built-in host pairs with Just Works and drops the old bond on repeat
  pairing, so the encrypted link does not authenticate the phone: any nearby device can bond and send
  START. Image authenticity has to come from secure boot signing, and downgrades from anti-rollback
  (secure version) or a version check in the application.
- **Several connections.** The transfer is not bound to the connection that started it: the
  connection handle, notification flags and disconnect handling are global. This only matters when
  the application accepts more than one connection; fixing it changes `esp_ble_ota_on_disconnect()`
  and `esp_ble_ota_on_subscribe()`.
