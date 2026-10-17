/*
** Julien ROIRON, 2026
** test_ble_ota.c
** File description:
** Unit tests of the BLE OTA protocol (see PROTOCOL.md)
*/

#include <string.h>
#include <sys/param.h>
#include <unity.h>

#include "ble_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"

#define CONN_HANDLE 1
#define ACK_LENGTH 20
#define LAST_PACKET 0xff
#define SECTOR_SIZE 4096
#define CHUNK 512

#define CMD_START 0x0001
#define CMD_STOP 0x0002
#define CMD_ACK 0x0003
#define ACK_ACCEPTED 0x0000
#define ACK_REFUSED 0x0001
#define SECTOR_STORED 0x0000
#define SECTOR_RESEND 0x0002
#define SECTOR_ABORTED 0x0003

/* Required by the component (declared extern in src/). */
SemaphoreHandle_t notify_sem;

static const uint8_t fw[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static struct {
    esp_err_t start_ret;
    esp_err_t sector_ret;
    uint32_t start_length;
    uint8_t sector[sizeof(fw)];
    uint32_t sector_length;
    int sectors;
    int ends;
    int aborts;
    int acks;
    uint16_t ack_handle;
    uint8_t ack[ACK_LENGTH];
} rec;

struct os_mbuf *__wrap_ble_hs_mbuf_from_flat(const void *buf, uint16_t len)
{
    static struct os_mbuf om;

    TEST_ASSERT_EQUAL_UINT16(ACK_LENGTH, len);
    memcpy(rec.ack, buf, ACK_LENGTH);
    return &om;
}

int __wrap_ble_gattc_notify_custom(uint16_t conn_handle, uint16_t att_handle, struct os_mbuf *om)
{
    TEST_ASSERT_EQUAL_UINT16(CONN_HANDLE, conn_handle);
    rec.ack_handle = att_handle;
    rec.acks++;
    return 0;
}

static esp_err_t on_start(uint32_t fw_length)
{
    rec.start_length = fw_length;
    return rec.start_ret;
}

static esp_err_t on_sector(uint8_t *buf, uint32_t length)
{
    rec.sectors++;
    rec.sector_length = length;
    memcpy(rec.sector, buf, MIN(length, sizeof(rec.sector)));
    return rec.sector_ret;
}

static void on_end(void)
{
    rec.ends++;
}

static void on_abort(void)
{
    rec.aborts++;
}

static uint16_t chr_handle(esp_ble_ota_char_t chr)
{
    return *esp_ble_ota_gatt_svcs()->characteristics[chr].val_handle;
}

static int write_mbuf(esp_ble_ota_char_t chr, struct os_mbuf *om)
{
    const struct ble_gatt_chr_def *def = &esp_ble_ota_gatt_svcs()->characteristics[chr];
    struct ble_gatt_access_ctxt ctxt = {.op = BLE_GATT_ACCESS_OP_WRITE_CHR, .om = om};

    return def->access_cb(CONN_HANDLE, *def->val_handle, &ctxt, def->arg);
}

static int write_chr(esp_ble_ota_char_t chr, const uint8_t *data, uint16_t len)
{
    struct os_mbuf om = {.om_data = (uint8_t *)data, .om_len = len};

    return write_mbuf(chr, &om);
}

static int send_command(uint16_t cmd, uint32_t fw_length)
{
    uint8_t buf[ACK_LENGTH] = {cmd & 0xff,
                               cmd >> 8,
                               fw_length & 0xff,
                               (fw_length >> 8) & 0xff,
                               (fw_length >> 16) & 0xff,
                               fw_length >> 24};

    return write_chr(CMD_CHAR, buf, sizeof(buf));
}

static uint16_t crc16_ccitt(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0;

    while (len--) {
        crc ^= *buf++ << 8;
        for (int i = 0; i < 8; i++)
            crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

/* Sectors start at fw[0]: the last packet ends with the CRC16 of fw up to the end of its data. */
static int send_packet(uint16_t sector, uint8_t packet, const uint8_t *data, uint16_t len)
{
    uint8_t buf[3 + sizeof(fw) + 2] = {sector & 0xff, sector >> 8, packet};
    uint16_t crc = crc16_ccitt(fw, data + len - fw);

    memcpy(&buf[3], data, len);
    if (packet != LAST_PACKET)
        return write_chr(RECV_FW_CHAR, buf, 3 + len);
    buf[3 + len] = crc & 0xff;
    buf[4 + len] = crc >> 8;
    return write_chr(RECV_FW_CHAR, buf, 5 + len);
}

/* Sends one sector in CHUNK-byte packets, the last packet ending with the sector CRC16. */
static int send_sector(uint16_t sector, const uint8_t *data, size_t len)
{
    uint8_t buf[3 + CHUNK + 2] = {sector & 0xff, sector >> 8};
    uint16_t crc = crc16_ccitt(data, len);
    int rc = 0;

    for (uint8_t packet = 0; len > CHUNK && rc == 0; packet++, data += CHUNK, len -= CHUNK) {
        buf[2] = packet;
        memcpy(&buf[3], data, CHUNK);
        rc = write_chr(RECV_FW_CHAR, buf, 3 + CHUNK);
    }
    if (rc != 0)
        return rc;
    buf[2] = LAST_PACKET;
    memcpy(&buf[3], data, len);
    buf[3 + len] = crc & 0xff;
    buf[4 + len] = crc >> 8;
    return write_chr(RECV_FW_CHAR, buf, 5 + len);
}

/* Checks the last notification: three little-endian fields, zeros, then the CRC16. */
static void assert_ack(esp_ble_ota_char_t chr, uint16_t field0, uint16_t field1, uint16_t field2)
{
    uint8_t expected[ACK_LENGTH] = {field0 & 0xff, field0 >> 8,   field1 & 0xff,
                                    field1 >> 8,   field2 & 0xff, field2 >> 8};
    uint16_t crc = crc16_ccitt(expected, ACK_LENGTH - 2);

    expected[ACK_LENGTH - 2] = crc & 0xff;
    expected[ACK_LENGTH - 1] = crc >> 8;
    TEST_ASSERT_EQUAL_UINT16(chr_handle(chr), rec.ack_handle);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, rec.ack, ACK_LENGTH);
}

void setUp(void)
{
    const struct ble_gatt_chr_def *chr = esp_ble_ota_gatt_svcs()->characteristics;

    if (notify_sem == NULL)
        notify_sem = xSemaphoreCreateCounting(1, 1);
    /* Resets the transfer left by the previous test. */
    esp_ble_ota_on_disconnect();
    memset(&rec, 0, sizeof(rec));
    esp_ble_ota_set_start_cb(on_start);
    esp_ble_ota_set_sector_cb(on_sector);
    esp_ble_ota_set_end_cb(NULL);
    esp_ble_ota_set_abort_cb(on_abort);

    /* Value handles, as NimBLE assigns them when the service is registered. */
    for (int i = 0; chr[i].uuid != NULL; i++)
        *chr[i].val_handle = i + 1;
    esp_ble_ota_on_connect(CONN_HANDLE);
    esp_ble_ota_on_subscribe(chr_handle(RECV_FW_CHAR), true);
    esp_ble_ota_on_subscribe(chr_handle(CMD_CHAR), true);
}

void tearDown(void)
{
}

TEST_CASE("START is acked and gives the firmware length", "[ble_ota]")
{
    TEST_ASSERT_EQUAL_INT(0, send_command(CMD_START, 5000));
    TEST_ASSERT_EQUAL_UINT32(5000, rec.start_length);
    TEST_ASSERT_EQUAL_UINT32(5000, esp_ble_ota_get_fw_length());
    assert_ack(CMD_CHAR, CMD_ACK, CMD_START, ACK_ACCEPTED);
}

TEST_CASE("START refused by the application blocks the firmware data", "[ble_ota]")
{
    rec.start_ret = ESP_FAIL;
    send_command(CMD_START, 5000);
    assert_ack(CMD_CHAR, CMD_ACK, CMD_START, ACK_REFUSED);
    TEST_ASSERT_EQUAL_UINT32(0, esp_ble_ota_get_fw_length());

    send_packet(0, LAST_PACKET, fw, sizeof(fw));
    TEST_ASSERT_EQUAL_INT(0, rec.sectors);
    TEST_ASSERT_EQUAL_INT(1, rec.acks);
}

TEST_CASE("START during a transfer aborts the previous one", "[ble_ota][security]")
{
    send_command(CMD_START, sizeof(fw));
    send_command(CMD_START, sizeof(fw));
    TEST_ASSERT_EQUAL_INT(1, rec.aborts);
    assert_ack(CMD_CHAR, CMD_ACK, CMD_START, ACK_ACCEPTED);
}

TEST_CASE("short command is rejected", "[ble_ota]")
{
    static const uint8_t start[] = {CMD_START, 0x00};

    TEST_ASSERT_EQUAL_INT(
        BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN, write_chr(CMD_CHAR, start, sizeof(start))
    );
    TEST_ASSERT_EQUAL_INT(0, rec.acks);
    TEST_ASSERT_EQUAL_UINT32(0, esp_ble_ota_get_fw_length());
}

TEST_CASE("sectors are given to the application and acked", "[ble_ota]")
{
    send_command(CMD_START, sizeof(fw) + 4);
    TEST_ASSERT_EQUAL_INT(0, send_packet(0, 0, fw, 4));
    TEST_ASSERT_EQUAL_INT(0, send_packet(0, 1, fw + 4, 4));
    TEST_ASSERT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw + 8, 4));
    TEST_ASSERT_EQUAL_INT(1, rec.sectors);
    TEST_ASSERT_EQUAL_UINT32(sizeof(fw), rec.sector_length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(fw, rec.sector, sizeof(fw));
    assert_ack(RECV_FW_CHAR, 0, SECTOR_STORED, 0);

    /* The last sector may use the index 0xffff. */
    TEST_ASSERT_EQUAL_INT(0, send_packet(0xffff, LAST_PACKET, fw, 4));
    TEST_ASSERT_EQUAL_INT(2, rec.sectors);
    assert_ack(RECV_FW_CHAR, 0xffff, SECTOR_STORED, 0);
}

TEST_CASE("wrong sector index asks for the expected sector", "[ble_ota]")
{
    send_command(CMD_START, sizeof(fw));
    TEST_ASSERT_NOT_EQUAL_INT(0, send_packet(1, 0, fw, 4));
    TEST_ASSERT_EQUAL_INT(0, rec.sectors);
    assert_ack(RECV_FW_CHAR, 1, SECTOR_RESEND, 0);
}

TEST_CASE("out of order packet asks for the sector again", "[ble_ota]")
{
    send_command(CMD_START, sizeof(fw));
    send_packet(0, 0, fw, 4);
    TEST_ASSERT_NOT_EQUAL_INT(0, send_packet(0, 2, fw + 8, 4));
    TEST_ASSERT_NOT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw + 8, 4));
    TEST_ASSERT_EQUAL_INT(0, rec.sectors);
    assert_ack(RECV_FW_CHAR, 0, SECTOR_RESEND, 0);

    send_packet(0, 0, fw, 4);
    send_packet(0, 1, fw + 4, 4);
    TEST_ASSERT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw + 8, 4));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(fw, rec.sector, sizeof(fw));
    assert_ack(RECV_FW_CHAR, 0, SECTOR_STORED, 0);
}

TEST_CASE("lost packet before the last one asks for the sector again", "[ble_ota]")
{
    send_command(CMD_START, sizeof(fw));
    send_packet(0, 0, fw, 4);
    TEST_ASSERT_NOT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw + 8, 4));
    TEST_ASSERT_EQUAL_INT(0, rec.sectors);
    assert_ack(RECV_FW_CHAR, 0, SECTOR_RESEND, 0);
}

TEST_CASE("packet header is read from the whole mbuf chain", "[ble_ota][security]")
{
    /* os_mbuf_adj() can leave the emptied ATT header mbuf in front of the value. */
    uint8_t stale[3] = {0xaa, 0xaa, 0xaa};
    uint8_t pkt[3 + sizeof(fw) + 2] = {0, 0, LAST_PACKET};
    uint16_t crc = crc16_ccitt(fw, sizeof(fw));
    struct os_mbuf value = {.om_data = pkt, .om_len = sizeof(pkt)};
    struct os_mbuf head = {.om_data = stale, .om_len = 0};

    memcpy(&pkt[3], fw, sizeof(fw));
    pkt[3 + sizeof(fw)] = crc & 0xff;
    pkt[4 + sizeof(fw)] = crc >> 8;
    SLIST_NEXT(&head, om_next) = &value;

    send_command(CMD_START, sizeof(fw));
    TEST_ASSERT_EQUAL_INT(0, write_mbuf(RECV_FW_CHAR, &head));
    TEST_ASSERT_EQUAL_INT(1, rec.sectors);
    assert_ack(RECV_FW_CHAR, 0, SECTOR_STORED, 0);
}

TEST_CASE("sector larger than 4096 bytes is refused", "[ble_ota][security]")
{
    static uint8_t data[SECTOR_SIZE + 2];

    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = i;
    send_command(CMD_START, 3 * SECTOR_SIZE);
    TEST_ASSERT_EQUAL_INT(0, send_sector(0, data, SECTOR_SIZE));
    TEST_ASSERT_EQUAL_UINT32(SECTOR_SIZE, rec.sector_length);

    TEST_ASSERT_NOT_EQUAL_INT(0, send_sector(1, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(1, rec.sectors);
}

TEST_CASE("sector refused by the application is asked again", "[ble_ota]")
{
    rec.sector_ret = ESP_FAIL;
    send_command(CMD_START, sizeof(fw));
    TEST_ASSERT_NOT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw, sizeof(fw)));
    assert_ack(RECV_FW_CHAR, 0, SECTOR_RESEND, 0);

    rec.sector_ret = ESP_OK;
    TEST_ASSERT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw, sizeof(fw)));
    TEST_ASSERT_EQUAL_INT(2, rec.sectors);
    assert_ack(RECV_FW_CHAR, 0, SECTOR_STORED, 0);
}

TEST_CASE("STOP is acked at once without an end callback", "[ble_ota]")
{
    send_command(CMD_START, sizeof(fw));
    send_packet(0, LAST_PACKET, fw, sizeof(fw));
    TEST_ASSERT_EQUAL_INT(0, send_command(CMD_STOP, 0));
    TEST_ASSERT_EQUAL_UINT32(0, esp_ble_ota_get_fw_length());
    assert_ack(CMD_CHAR, CMD_ACK, CMD_STOP, ACK_ACCEPTED);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, esp_ble_ota_end_ack(ESP_OK));
}

TEST_CASE("STOP ack waits for the application verdict", "[ble_ota]")
{
    esp_ble_ota_set_end_cb(on_end);
    send_command(CMD_START, sizeof(fw));
    send_packet(0, LAST_PACKET, fw, sizeof(fw));
    send_command(CMD_STOP, 0);
    TEST_ASSERT_EQUAL_INT(1, rec.ends);
    TEST_ASSERT_EQUAL_INT(2, rec.acks);

    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_ble_ota_end_ack(ESP_FAIL));
    assert_ack(CMD_CHAR, CMD_ACK, CMD_STOP, ACK_REFUSED);
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_STATE, esp_ble_ota_end_ack(ESP_OK));
}

TEST_CASE("STOP without a transfer is refused", "[ble_ota][security]")
{
    esp_ble_ota_set_end_cb(on_end);
    send_command(CMD_STOP, 0);
    TEST_ASSERT_EQUAL_INT(0, rec.ends);
    assert_ack(CMD_CHAR, CMD_ACK, CMD_STOP, ACK_REFUSED);
}

TEST_CASE("STOP before the whole firmware is received is refused", "[ble_ota][security]")
{
    esp_ble_ota_set_end_cb(on_end);
    send_command(CMD_START, 2 * sizeof(fw));
    send_packet(0, LAST_PACKET, fw, sizeof(fw));
    send_command(CMD_STOP, 0);
    TEST_ASSERT_EQUAL_INT(0, rec.ends);
    TEST_ASSERT_EQUAL_INT(1, rec.aborts);
    assert_ack(CMD_CHAR, CMD_ACK, CMD_STOP, ACK_REFUSED);
}

TEST_CASE("firmware beyond the announced length aborts the transfer", "[ble_ota][security]")
{
    send_command(CMD_START, 4);
    TEST_ASSERT_NOT_EQUAL_INT(0, send_packet(0, LAST_PACKET, fw, sizeof(fw)));
    TEST_ASSERT_EQUAL_INT(0, rec.sectors);
    TEST_ASSERT_EQUAL_INT(1, rec.aborts);
    assert_ack(RECV_FW_CHAR, 0, SECTOR_ABORTED, 0);
}

TEST_CASE("disconnect during a transfer aborts it", "[ble_ota]")
{
    esp_ble_ota_on_disconnect();
    TEST_ASSERT_EQUAL_INT(0, rec.aborts);

    send_command(CMD_START, sizeof(fw));
    esp_ble_ota_on_disconnect();
    TEST_ASSERT_EQUAL_INT(1, rec.aborts);
    TEST_ASSERT_EQUAL_UINT32(0, esp_ble_ota_get_fw_length());
}
