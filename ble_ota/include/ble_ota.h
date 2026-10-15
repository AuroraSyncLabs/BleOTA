/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef _BLE_OTA_H_
#define _BLE_OTA_H_

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
 * Called when the link drops while an OTA is in progress, so the
 * application can release its own OTA state (e.g. esp_ota_abort() or
 * esp_encrypted_img_decrypt_abort()).
 */
typedef void (*esp_ble_ota_abort_cb_t)(void);

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

#if CONFIG_BT_BLUEDROID_ENABLED
/// BLE OTA characteristics
typedef enum {
    RECV_FW_CHAR,
    RECV_FW_CHAR_CCC,

    OTA_STATUS_CHAR,
    OTA_STATUS_CHAR_CCC,

    CMD_CHAR,
    CMD_CHAR_CCC,

    CUS_CHAR,
    CUS_CHAR_CCC,

    INVALID_CHAR,
} esp_ble_ota_char_t;

/// BLE DIS characteristics
typedef enum {
    DIS_SVC_IDX,

    DIS_MODEL_CHAR_IDX,
    DIS_MODEL_CHAR_VAL_IDX,

    DIS_SN_CHAR_IDX,
    DIS_SN_CHAR_VAL_IDX,

    DIS_FW_CHAR_IDX,
    DIS_FW_CHAR_VAL_IDX,

    DIS_IDX_NB,
} esp_ble_dis_service_index_t;

/// BLE OTA characteristics Index
typedef enum {
    OTA_SVC_IDX,

    RECV_FW_CHAR_IDX,
    RECV_FW_CHAR_VAL_IDX,
    RECV_FW_CHAR_NTF_CFG,

    OTA_STATUS_CHAR_IDX,
    OTA_STATUS_CHAR_VAL_IDX,
    OTA_STATUS_NTF_CFG,

    CMD_CHAR_IDX,
    CMD_CHAR_VAL_IDX,
    CMD_CHAR_NTF_CFG,

    CUS_CHAR_IDX,
    CUS_CHAR_VAL_IDX,
    CUS_CHAR_NTF_CFG,

    OTA_IDX_NB,
} esp_ble_ota_service_index_t;

#else

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

/**
 * @brief           This function is called to process write event on
 * characteristics
 *
 * @return          ESP_OK on success, or an error (e.g. decrypt / protocol
 * failure under CONFIG_PRE_ENC_OTA).
 *
 */
esp_err_t esp_ble_ota_write(uint8_t *file, size_t length);

/**
 * @brief           This function is used to set total file size and each block
 * size
 *
 * @return          void
 *
 */
void esp_ble_ota_set_sizes(size_t file_size, size_t block_size);

#if !CONFIG_OTA_WITH_PROTOCOMM
#if CONFIG_BLE_OTA_EXTERNAL_HOST
/**
 * @brief           Register the OTA GATT service on an application-owned NimBLE
 * host
 *
 * Adds the service with ble_gatts_add_dynamic_svcs(). It does not touch GAP,
 * advertising, the device name or ble_hs_cfg. Call it after the host has synced
 * and before advertising starts (NimBLE refuses to change services while
 * connected or advertising).
 *
 * @return
 *                  - ESP_OK: success
 *                  - ESP_FAIL: service registration failed
 */
esp_err_t esp_ble_ota_gatt_register(void);
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
 * @param[in]       indicate : event->subscribe.cur_indicate.
 */
void esp_ble_ota_on_subscribe(uint16_t attr_handle, bool indicate);

/**
 * @brief           Set the callback invoked when the link drops during an OTA
 *
 * @param[in]       callback : application callback, or NULL to clear it.
 *
 * @return
 *                  - ESP_OK: success
 */
esp_err_t esp_ble_ota_set_abort_cb(esp_ble_ota_abort_cb_t callback);
#endif /* !CONFIG_OTA_WITH_PROTOCOMM */

#endif
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

/**
 * @brief           This function is called to indicate OTA end
 *
 */
void esp_ble_ota_finish(void);
#ifdef __cplusplus
}
#endif

#endif
