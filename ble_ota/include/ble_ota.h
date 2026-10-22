/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 AnabolicRoo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef _BLE_OTA_H_
#define _BLE_OTA_H_

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_PRE_ENC_OTA
#include "esp_encrypted_img.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BLE OAT receive data callback function type
 * @param buf : The pointer to the receive data buffer.
 * @param length : The length of receive data buffer.
 */
typedef void (*esp_ble_ota_recv_fw_cb_t)(uint8_t *buf, uint32_t length);

/**
 * @brief BLE OTA abort callback function type
 *
 * Called from the NimBLE host task when an OTA in progress is dropped, so the
 * application can release its own OTA state (e.g. esp_ota_abort()): the link
 * dropped, a new START superseded it, the firmware went beyond the announced
 * length, the STOP came before the whole firmware or, under
 * CONFIG_PRE_ENC_OTA, a sector could not be decrypted or stored, or the image
 * failed its authentication at STOP. The decrypt handle is released by the
 * component.
 */
typedef void (*esp_ble_ota_abort_cb_t)(void);

/**
 * @brief BLE OTA start callback function type
 *
 * Called from the NimBLE host task when the peer sends the START command.
 * Return an error to refuse the transfer: the START ack then reports
 * "refused" and no firmware data is accepted.
 *
 * @param fw_length : firmware length announced by the peer (decrypted size
 *                    under CONFIG_PRE_ENC_OTA).
 */
typedef esp_err_t (*esp_ble_ota_start_cb_t)(uint32_t fw_length);

/**
 * @brief BLE OTA sector callback function type
 *
 * Same as esp_ble_ota_recv_fw_cb_t, but the application reports whether it
 * stored the sector. On error the sector is NACKed and the peer resends it.
 *
 * Under CONFIG_PRE_ENC_OTA the data is the decrypted firmware, in chunks of
 * any size up to one sector. A refused chunk cannot be resent (the AES-GCM
 * state already advanced): an error aborts the transfer, the peer is told to
 * stop and the abort callback is called.
 *
 * @param buf : The pointer to the sector data.
 * @param length : The length of the sector data.
 */
typedef esp_err_t (*esp_ble_ota_sector_cb_t)(uint8_t *buf, uint32_t length);

/**
 * @brief BLE OTA end callback function type
 *
 * Called from the NimBLE host task when the peer sends the STOP command. The
 * STOP ack is held back until the application calls esp_ble_ota_end_ack(),
 * so the peer learns whether the image was accepted.
 */
typedef void (*esp_ble_ota_end_cb_t)(void);

/**
 * @brief BLE OTA callback functions struct
 */
typedef struct esp_ble_ota_callback_funs {
    esp_ble_ota_recv_fw_cb_t recv_fw_cb; /*!< BLE OTA data receive callback function */
} esp_ble_ota_callback_funs_t;

/**
 * @brief BLE OTA notification flags struct
 */
typedef struct esp_ble_ota_notification_check {
    bool recv_fw_ntf_enable;     /*!< BLE OTA receive firmware characteristic */
    bool process_bar_ntf_enable; /*!< BLE OTA notify process bar
                                    characteristic */
    bool command_ntf_enable;     /*!< BLE OTA command characteristic */
    bool customer_ntf_enable;    /*!< BLE OTA customer data characteristic */
} esp_ble_ota_notification_check_t;

typedef enum {
    RECV_FW_CHAR,
    OTA_STATUS_CHAR,
    CMD_CHAR,
    CUS_CHAR,
    INVALID_CHAR,
} esp_ble_ota_char_t;

typedef enum {
    RECV_FW_CHAR_VAL_IDX,
    OTA_STATUS_CHAR_VAL_IDX,
    CMD_CHAR_VAL_IDX,
    CUS_CHAR_VAL_IDX,
} esp_ble_ota_service_index_t;

#if CONFIG_BLE_OTA_EXTERNAL_HOST
struct ble_gatt_svc_def;

/**
 * @brief           OTA GATT service table, for an application-owned NimBLE host
 *
 * Register it with the application's own services before ble_gatts_start(),
 * through ble_gatts_count_cfg() and ble_gatts_add_svcs() (or the owner of the
 * host, e.g. Matter's ConfigureExtraServices()). Counting it up front sizes
 * the NimBLE client configuration pool for its characteristics.
 *
 * @return          The service table, terminated by an empty entry.
 */
const struct ble_gatt_svc_def *esp_ble_ota_gatt_svcs(void);

#if CONFIG_BT_NIMBLE_DYNAMIC_SERVICE
/**
 * @brief           Register the OTA GATT service on an application-owned NimBLE
 * host
 *
 * Adds the service with ble_gatts_add_dynamic_svcs(). It does not touch GAP,
 * advertising, the device name or ble_hs_cfg. Call it after the host has synced
 * and before advertising starts (NimBLE refuses to change services while
 * connected or advertising).
 *
 * NimBLE sizes its client configuration pool when the GATT server starts:
 * notifying characteristics added later can exhaust it, after which
 * connectable advertising fails with BLE_HS_ENOMEM. Prefer
 * esp_ble_ota_gatt_svcs() when the services can be registered up front.
 *
 * @return
 *                  - ESP_OK: success
 *                  - ESP_FAIL: service registration failed
 */
esp_err_t esp_ble_ota_gatt_register(void);
#endif
#endif

/**
 * @brief           Forward BLE_GAP_EVENT_CONNECT (status 0) to the OTA profile
 *
 * @param[in]       conn_handle : handle of the new connection.
 */
void esp_ble_ota_on_connect(uint16_t conn_handle);

/**
 * @brief           Forward BLE_GAP_EVENT_DISCONNECT to the OTA profile
 *
 * Resets the transfer state, frees the sector buffer and, if an OTA was in
 * progress, calls the callback set with esp_ble_ota_set_abort_cb().
 */
void esp_ble_ota_on_disconnect(void);

/**
 * @brief           Forward BLE_GAP_EVENT_SUBSCRIBE to the OTA profile
 *
 * @param[in]       attr_handle : event->subscribe.attr_handle.
 * @param[in]       notify : event->subscribe.cur_notify.
 */
void esp_ble_ota_on_subscribe(uint16_t attr_handle, bool notify);

/**
 * @brief           Set the callback invoked when the link drops during an OTA
 *
 * @param[in]       callback : application callback, or NULL to clear it.
 *
 * @return
 *                  - ESP_OK: success
 */
esp_err_t esp_ble_ota_set_abort_cb(esp_ble_ota_abort_cb_t callback);

/**
 * @brief           Set the callback invoked when the peer sends START
 *
 * @param[in]       callback : application callback, or NULL to clear it.
 *
 * @return
 *                  - ESP_OK: success
 */
esp_err_t esp_ble_ota_set_start_cb(esp_ble_ota_start_cb_t callback);

/**
 * @brief           Set the callback receiving each firmware sector
 *
 * When set, it is used instead of the callback registered with
 * esp_ble_ota_recv_fw_data_callback(). Under CONFIG_PRE_ENC_OTA a refused
 * chunk aborts the transfer (see esp_ble_ota_sector_cb_t).
 *
 * @param[in]       callback : application callback, or NULL to clear it.
 *
 * @return
 *                  - ESP_OK: success
 */
esp_err_t esp_ble_ota_set_sector_cb(esp_ble_ota_sector_cb_t callback);

/**
 * @brief           Set the callback invoked when the peer sends STOP
 *
 * When set, the STOP ack is deferred until esp_ble_ota_end_ack() is called.
 *
 * @param[in]       callback : application callback, or NULL to clear it.
 *
 * @return
 *                  - ESP_OK: success
 */
esp_err_t esp_ble_ota_set_end_cb(esp_ble_ota_end_cb_t callback);

#ifdef CONFIG_PRE_ENC_OTA
/**
 * @brief           Set the key decrypting pre-encrypted images
 *
 * The component then opens a decrypt handle on every START and releases it
 * on STOP, on a refused or superseded START, on a decrypt error and on
 * disconnect. At STOP the image length and authentication tag are checked:
 * on failure the end callback is not called, the abort callback is, and the
 * STOP ack reports "refused".
 *
 * Call before the peer can connect. The configuration is copied, but the key
 * it points to must stay valid while the component is in use.
 *
 * @param[in]       cfg : decrypt configuration (e.g. RSA private key in PEM).
 *
 * @return
 *                  - ESP_OK: success
 *                  - ESP_ERR_INVALID_ARG: cfg is NULL
 */
esp_err_t esp_ble_ota_set_decrypt_cfg(const esp_decrypt_cfg_t *cfg);
#endif

/**
 * @brief           Send the deferred STOP ack
 *
 * Call once the image has been verified (e.g. after esp_ota_end()). May be
 * called from any task.
 *
 * @param[in]       result : ESP_OK to accept the image, any error to report
 *                           it as refused.
 *
 * @return
 *                  - ESP_OK: ack sent
 *                  - ESP_ERR_INVALID_STATE: no STOP ack pending (none received,
 *                    already acked, or the link dropped)
 *                  - ESP_FAIL: the notification could not be sent
 */
esp_err_t esp_ble_ota_end_ack(esp_err_t result);

#if !CONFIG_BLE_OTA_EXTERNAL_HOST
/**
 * @brief           This function is called to Initialization ble ota host
 *
 * @return
 *                  - ESP_OK: success
 *                  - other: failed
 *
 */
esp_err_t esp_ble_ota_host_init(void);
#endif

/**
 * @brief           This function is called to register ble ota receive firmware
 * data callback function
 *
 * @param[in]       callback : pointer to the application callback function.
 *
 * Under CONFIG_PRE_ENC_OTA, esp_decrypt_handle is used for the next transfer
 * only, unless esp_ble_ota_set_decrypt_cfg() was called: the component takes
 * ownership and releases it at STOP, on a decrypt error or on disconnect.
 *
 * @return
 *                  - ESP_OK: success
 *                  - other: failed
 *
 */
#ifdef CONFIG_PRE_ENC_OTA
esp_err_t esp_ble_ota_recv_fw_data_callback(
    esp_ble_ota_recv_fw_cb_t callback,
    esp_decrypt_handle_t esp_decrypt_handle
);
#else
esp_err_t esp_ble_ota_recv_fw_data_callback(esp_ble_ota_recv_fw_cb_t callback);
#endif

/**
 * @brief           This function is called to Initialization ble ota
 * process
 *
 * @return
 *                  - length of ota firmware
 *
 */
unsigned int esp_ble_ota_get_fw_length(void);

#ifdef __cplusplus
}
#endif

#endif
