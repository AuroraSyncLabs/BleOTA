#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "bleak>=0.22",
#     "cryptography>=42",
# ]
# ///
"""Send a firmware image to a device running the ble_ota component.

Follows ble_ota/PROTOCOL.md: connect, pair, subscribe, START, 4096-byte sectors
acked one by one, STOP.

Examples:
    uv run tools/ble_ota.py build/minimal.bin
    uv run tools/ble_ota.py app.bin --address AA:BB:CC:DD:EE:FF
    uv run tools/ble_ota.py app.bin --key rsa_key.pem   # firmware built with CONFIG_PRE_ENC_OTA
"""

import argparse
import asyncio
import binascii
import os
import struct
import sys
import time
import warnings
from pathlib import Path

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.backends.device import BLEDevice
from bleak.exc import BleakError
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

DEFAULT_DEVICE_NAME = "nimble-ble-ota"
RECV_FW_UUID = "00008020-0000-1000-8000-00805f9b34fb"
COMMAND_UUID = "00008022-0000-1000-8000-00805f9b34fb"

SECTOR_SIZE = 4096
PACKET_HEADER_SIZE = 3
SECTOR_CRC_SIZE = 2
LAST_PACKET = 0xFF
ATT_HEADER_SIZE = 3

CMD_START = 0x0001
CMD_STOP = 0x0002
CMD_ACK = 0x0003
CMD_ACCEPTED = 0x0000

SECTOR_STORED = 0x0000
SECTOR_RESEND = 0x0002
SECTOR_ABORTED = 0x0003

# esp_encrypted_img v2 (RSA-3072) image layout, as written by esp_enc_img_gen.py.
ENC_MAGIC = 0x0788B6CF
ENC_HEADER_SIZE = 512
ENC_RESERVED_SIZE = ENC_HEADER_SIZE - (4 + 384 + 16 + 4 + 16)
ESP_IMAGE_MAGIC = 0xE9

START_ACK_TIMEOUT_S = 10.0
# Pre-encrypted OTA: the first sector also pays for the RSA decryption of the image key.
SECTOR_ACK_TIMEOUT_S = 15.0
# The application can verify the whole image before acking (esp_ble_ota_set_end_cb()).
STOP_ACK_TIMEOUT_S = 60.0
MAX_SECTOR_RETRIES = 5


class OtaError(Exception):
    pass


def crc16_ccitt(data: bytes) -> int:
    return binascii.crc_hqx(data, 0)


def build_command(command: int, length: int = 0) -> bytes:
    body = struct.pack("<HI", command, length).ljust(18, b"\x00")
    return body + struct.pack("<H", crc16_ccitt(body))


def encrypt_image(plain: bytes, key_path: Path) -> bytes:
    """Encrypt like `esp_enc_img_gen.py encrypt`."""
    pem = key_path.read_bytes()
    if b"PRIVATE KEY" in pem:
        public_key = serialization.load_pem_private_key(pem, password=None).public_key()
    else:
        public_key = serialization.load_pem_public_key(pem)
    if not isinstance(public_key, rsa.RSAPublicKey) or public_key.key_size != 3072:
        raise OtaError(f"{key_path}: expected an RSA-3072 key")

    gcm_key = os.urandom(32)
    iv = os.urandom(16)
    sealed = AESGCM(gcm_key).encrypt(iv, plain, None)
    ciphertext, tag = sealed[: len(plain)], sealed[len(plain) :]
    header = (
        struct.pack("<I", ENC_MAGIC)
        + public_key.encrypt(gcm_key, padding.PKCS1v15())
        + iv
        + struct.pack("<I", len(ciphertext))
        + tag
        + bytes(ENC_RESERVED_SIZE)
    )
    return header + ciphertext


def load_image(firmware: Path, key: Path | None) -> bytes:
    image = firmware.read_bytes()
    if not image:
        raise OtaError(f"{firmware} is empty")
    if key is None:
        return image
    if image[0] != ESP_IMAGE_MAGIC:
        raise OtaError(f"{firmware} is not a plain ESP application image")
    image = encrypt_image(image, key)
    print(f"Encrypted {firmware.name} with {key.name}: {len(image)} bytes")
    return image


def split_sector(sector: bytes, payload_size: int) -> list[bytes]:
    """Split a sector into packet payloads; the last one leaves room for the CRC."""
    if payload_size <= SECTOR_CRC_SIZE:
        raise OtaError(f"packet payload of {payload_size} bytes is too small")
    chunks = [sector[i : i + payload_size] for i in range(0, len(sector), payload_size)]
    if len(chunks[-1]) + SECTOR_CRC_SIZE > payload_size:
        last = chunks.pop()
        cut = payload_size - SECTOR_CRC_SIZE
        chunks += [last[:cut], last[cut:]]
    if len(chunks) > LAST_PACKET:
        raise OtaError(f"{len(chunks)} packets per sector exceed the protocol limit")
    return chunks


def build_packets(index: int, sector: bytes, payload_size: int) -> list[bytes]:
    chunks = split_sector(sector, payload_size)
    packets = []
    for number, chunk in enumerate(chunks):
        last = number == len(chunks) - 1
        header = struct.pack("<HB", index, LAST_PACKET if last else number)
        trailer = struct.pack("<H", crc16_ccitt(sector)) if last else b""
        packets.append(header + chunk + trailer)
    return packets


async def find_device(args: argparse.Namespace) -> BLEDevice:
    if args.address:
        print(f"Scanning for {args.address} ...")
        device = await BleakScanner.find_device_by_address(args.address, timeout=args.scan_timeout)
    else:
        print(f"Scanning for '{args.name}' ...")
        device = await BleakScanner.find_device_by_name(args.name, timeout=args.scan_timeout)
    if device is None:
        raise OtaError("device not found (is it advertising? a phone may already be connected)")
    print(f"Found {device.name} [{device.address}]")
    return device


async def negotiated_mtu(client: BleakClient, char: BleakGATTCharacteristic) -> int:
    acquire_mtu = getattr(client._backend, "_acquire_mtu", None)
    if acquire_mtu is not None:  # BlueZ only reports the MTU after this
        try:
            await acquire_mtu()
        except Exception as err:  # private bleak API: fall back to the characteristic below
            print(f"Warning: cannot read the ATT MTU from BlueZ ({err})")
    with warnings.catch_warnings():  # bleak warns when it falls back to 23
        warnings.simplefilter("ignore")
        mtu = client.mtu_size
    # BlueZ reads the characteristic's value at service discovery, before the MTU
    # exchange: it can be stale, keep the larger of both.
    return max(mtu, char.max_write_without_response_size + ATT_HEADER_SIZE)


async def wait_for(queue: asyncio.Queue[bytes], timeout: float, what: str) -> bytes:
    try:
        return await asyncio.wait_for(queue.get(), timeout)
    except TimeoutError:
        raise OtaError(f"no {what} within {timeout:.0f} s") from None


async def send_command(
    client: BleakClient, acks: asyncio.Queue[bytes], command: int, length: int, timeout: float
) -> None:
    name = "START" if command == CMD_START else "STOP"
    await client.write_gatt_char(COMMAND_UUID, build_command(command, length), response=True)
    while True:
        ack = await wait_for(acks, timeout, f"{name} ack")
        if len(ack) < 6:
            continue
        kind, acked, status = struct.unpack_from("<HHH", ack)
        if kind != CMD_ACK or acked != command:
            continue
        if status != CMD_ACCEPTED:
            raise OtaError(f"{name} refused by the device (see its log)")
        return


async def send_image(
    client: BleakClient, acks: asyncio.Queue[bytes], image: bytes, payload_size: int, response: bool
) -> None:
    sectors = [image[i : i + SECTOR_SIZE] for i in range(0, len(image), SECTOR_SIZE)]
    if len(sectors) > 0xFFFF:
        raise OtaError("image too large for 16-bit sector indexes")
    started = time.monotonic()
    index = 0
    retries = 0

    while index < len(sectors):
        for packet in build_packets(index, sectors[index], payload_size):
            await client.write_gatt_char(RECV_FW_UUID, packet, response=response)

        while True:
            ack = await wait_for(acks, SECTOR_ACK_TIMEOUT_S, f"ack for sector {index}")
            if len(ack) < 6:
                continue
            acked, status, resend = struct.unpack_from("<HHH", ack)
            if acked == index:
                break

        if status == SECTOR_STORED:
            index += 1
            retries = 0
            sent = min(index * SECTOR_SIZE, len(image))
            rate = sent / max(time.monotonic() - started, 1e-3) / 1024
            print(
                f"\r  {sent * 100 // len(image):3d}%  {sent}/{len(image)} bytes  {rate:6.1f} KiB/s",
                end="",
                flush=True,
            )
        elif status == SECTOR_RESEND:
            retries += 1
            if retries > MAX_SECTOR_RETRIES:
                raise OtaError(f"sector {resend} refused {MAX_SECTOR_RETRIES} times")
            print(f"\n  device asks to resend from sector {resend}")
            index = resend
        elif status == SECTOR_ABORTED:
            raise OtaError(f"device aborted the transfer at sector {index} (see its log)")
        else:
            raise OtaError(f"unknown sector ack status 0x{status:04x}")
    print()


async def run(args: argparse.Namespace) -> None:
    image = load_image(args.firmware, args.key)
    device = await find_device(args)

    async with BleakClient(device, timeout=args.connect_timeout) as client:
        print("Connected, pairing ...")
        try:
            await client.pair()
        except (BleakError, NotImplementedError) as err:
            print(f"Warning: explicit pairing failed ({err}), relying on the OS to pair on demand")

        char = client.services.get_characteristic(RECV_FW_UUID)
        if char is None:
            raise OtaError("OTA service not found on the device")
        sector_acks: asyncio.Queue[bytes] = asyncio.Queue()
        command_acks: asyncio.Queue[bytes] = asyncio.Queue()
        try:
            await client.start_notify(char, lambda _, data: sector_acks.put_nowait(bytes(data)))
            await client.start_notify(
                COMMAND_UUID, lambda _, data: command_acks.put_nowait(bytes(data))
            )
        except BleakError as err:
            raise OtaError(
                f"cannot subscribe to the OTA service ({err}). If the device or this computer "
                f"lost the bond, remove it on both sides (bluetoothctl remove {device.address})"
            ) from err

        mtu = await negotiated_mtu(client, char)
        packet_size = mtu - ATT_HEADER_SIZE
        if args.packet_size:
            packet_size = min(packet_size, args.packet_size)
        payload_size = packet_size - PACKET_HEADER_SIZE
        print(f"ATT MTU {mtu}, {payload_size}-byte payload per packet")

        print(f"START ({len(image)} bytes)")
        await send_command(client, command_acks, CMD_START, len(image), START_ACK_TIMEOUT_S)
        await send_image(client, sector_acks, image, payload_size, args.with_response)
        print("STOP, waiting for the device to accept the image ...")
        await send_command(client, command_acks, CMD_STOP, 0, STOP_ACK_TIMEOUT_S)
        print("Image accepted")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("firmware", type=Path, help="image to send (.bin)")
    target = parser.add_mutually_exclusive_group()
    target.add_argument(
        "--name", default=DEFAULT_DEVICE_NAME, help="advertised name (default: %(default)s)"
    )
    target.add_argument("--address", help="device address (or CoreBluetooth UUID on macOS)")
    parser.add_argument(
        "--key", type=Path, help="RSA-3072 PEM key to encrypt the image for CONFIG_PRE_ENC_OTA"
    )
    parser.add_argument("--packet-size", type=int, help="cap the packet size (default: MTU - 3)")
    parser.add_argument(
        "--with-response", action="store_true", help="write packets with response (slower)"
    )
    parser.add_argument("--scan-timeout", type=float, default=15.0, help="seconds (default: 15)")
    parser.add_argument("--connect-timeout", type=float, default=20.0, help="seconds (default: 20)")
    args = parser.parse_args()
    if args.packet_size is not None and args.packet_size <= PACKET_HEADER_SIZE + SECTOR_CRC_SIZE:
        parser.error(f"--packet-size must exceed {PACKET_HEADER_SIZE + SECTOR_CRC_SIZE} bytes")
    return args


def main() -> int:
    args = parse_args()
    try:
        asyncio.run(run(args))
    except (OtaError, BleakError, OSError) as err:
        print(f"\nError: {err}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nInterrupted (the device aborts the update on disconnect)", file=sys.stderr)
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
