set(srcs "")
set(requires bt nvs_flash)

if(CONFIG_BT_BLUEDROID_ENABLED)
    message(FATAL_ERROR "ble_ota supports NimBLE only: use espressif/ble_ota with Bluedroid")
endif()

if(CONFIG_BT_NIMBLE_ENABLED)
    list(APPEND srcs "src/nimble_ota.c")

    # ble_ota.h includes esp_encrypted_img.h.
    if(CONFIG_PRE_ENC_OTA)
        list(APPEND requires espressif__esp_encrypted_img)
    endif()
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       REQUIRES "${requires}")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
