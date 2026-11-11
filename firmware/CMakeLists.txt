cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Components are grouped into core/ (portable, no ESP-IDF), drivers/ (hardware) and
# services/ (*_service product policy); ESP-IDF only scans one level deep, so list each group.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/core"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers"
    "${CMAKE_CURRENT_LIST_DIR}/components/services"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(chroninkle)
