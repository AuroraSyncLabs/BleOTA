# Unit test app: runs the test cases of ../test with the Unity menu.
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS ../test)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(ble_ota_test)

# Coverage of the component, dumped by the [coverage] test case (no file system for libgcov).
idf_component_get_property(ble_ota_lib ble_ota COMPONENT_LIB)
target_compile_options(${ble_ota_lib} PRIVATE --coverage -fprofile-info-section=gcov_info)
target_link_options(${ble_ota_lib} INTERFACE --coverage)
