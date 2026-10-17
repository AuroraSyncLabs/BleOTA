idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity ble_ota bt
    WHOLE_ARCHIVE)

# No BLE controller on QEMU: the tests capture the notifications instead of sending them.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=ble_hs_mbuf_from_flat"
    "-Wl,--wrap=ble_gattc_notify_custom")
