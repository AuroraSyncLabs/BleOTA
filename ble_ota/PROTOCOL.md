# BLE OTA protocol

All multi-byte fields are little-endian.

## GATT

Service `0x8018`. Every characteristic requires an encrypted (bonded) link.

| UUID     | Name     | Properties                    | Use                        |
|----------|----------|-------------------------------|----------------------------|
| `0x8020` | RECV_FW  | Write, Write No Rsp, Notify   | Firmware data, sector acks |
| `0x8021` | STATUS   | Read, Notify                  | Unused                     |
| `0x8022` | COMMAND  | Write, Notify                 | START / STOP, command acks |
| `0x8023` | CUSTOMER | Write, Notify                 | Unused                     |

Enable notifications (CCCD `0x0001`) on RECV_FW and COMMAND before starting.

## Flow

1. Connect, bond, request MTU 517, subscribe.
2. Write START on COMMAND, wait for its ack. Refused: stop.
3. Send the firmware in 4096-byte sectors (the last one may be shorter, a longer one is rejected). After
   each sector, wait for its ack:
   - `0x0000`: send the next sector.
   - `0x0002`: resend from the sector given in the ack.
   - `0x0003`: transfer aborted (the sector goes beyond the announced length or, with pre-encrypted
     OTA, could not be decrypted or stored): stop.
4. Write STOP on COMMAND, wait for its ack (sent once the image is verified, can take a few seconds).
   - Accepted: the device reboots into the new firmware.
   - Refused: the update failed, nothing changed. A STOP is refused when no transfer is running or
     when fewer bytes than announced were received.
5. Reconnect and check the firmware version.

A disconnect aborts the transfer: restart from START.

## Command (write on COMMAND, 20 bytes)

| Bytes  | Field                                   |
|--------|-----------------------------------------|
| 0–1    | Command: `0x0001` START, `0x0002` STOP  |
| 2–5    | START only: file length (u32)           |
| 6–17   | `0x00`                                  |
| 18–19  | CRC16 of bytes 0–17 (not checked)       |

A shorter write is rejected with the ATT error `0x0D` (invalid attribute value length).

Firmware beyond this length aborts the transfer. With pre-encrypted OTA, the length is the size of
the encrypted file. A STOP is then refused when the decrypted image is incomplete or fails its
authentication.

## Command ack (notification on COMMAND, 20 bytes)

| Bytes  | Field                                   |
|--------|-----------------------------------------|
| 0–1    | `0x0003`                                |
| 2–3    | Acked command (`0x0001` / `0x0002`)     |
| 4–5    | `0x0000` accepted, `0x0001` refused     |
| 6–17   | `0x00`                                  |
| 18–19  | CRC16 of bytes 0–17                     |

## Firmware packet (write on RECV_FW)

A sector is split into packets of at most MTU − 3 bytes:

| Bytes  | Field                                                          |
|--------|----------------------------------------------------------------|
| 0–1    | Sector index, from 0 (the last sector may use `0xFFFF`)        |
| 2      | Packet index in the sector, from 0; `0xFF` for the last packet |
| 3–     | Firmware data                                                  |
| last 2 | Last packet only: CRC16 of the sector's firmware data          |

Packets of a sector must arrive in order. An out-of-order packet or a wrong sector CRC (e.g. a lost packet) makes the device ask for the whole sector again.

## Sector ack (notification on RECV_FW, 20 bytes)

| Bytes  | Field                                              |
|--------|----------------------------------------------------|
| 0–1    | Sector index received                              |
| 2–3    | `0x0000` stored, `0x0002` resend, `0x0003` aborted |
| 4–5    | Resend only: sector index to send next             |
| 6–17   | `0x00`                                             |
| 18–19  | CRC16 of bytes 0–17                                |

## CRC16

CRC16-CCITT: polynomial `0x1021`, initial value `0x0000`, no reflection, no final XOR.
