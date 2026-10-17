# Unit test app: runs the test cases of ../test with the Unity menu.
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS ../test)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(ble_ota_test)
