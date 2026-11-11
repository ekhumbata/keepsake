# Host unit tests + fuzzers for the portable firmware/components/core/ components.
#
# core/ never includes ESP-IDF headers, so it builds with the host compiler. This project
# compiles those same sources (never copies) into a static library and links the tests
# against it. See tests/README.md.
cmake_minimum_required(VERSION 3.16)
project(keepsake_tests CXX)

# ESP-IDF 5.5 builds the firmware as gnu++2b and core/ relies on C++20 features (defaulted
# operator==), so the host build needs at least C++20 too.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(KEEPSAKE_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" ON)
option(KEEPSAKE_FUZZ "Build the libFuzzer targets (needs clang)" OFF)

set(KEEPSAKE_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../firmware/components/core")
get_filename_component(KEEPSAKE_CORE_DIR "${KEEPSAKE_CORE_DIR}" ABSOLUTE)

# Every .cpp in every core component, plus each component's include dir (or the component dir
# itself, for the ones that register INCLUDE_DIRS "."). New core components are picked up
# automatically.
file(GLOB KEEPSAKE_CORE_SOURCES CONFIGURE_DEPENDS "${KEEPSAKE_CORE_DIR}/*/*.cpp")
file(GLOB KEEPSAKE_CORE_COMPONENTS LIST_DIRECTORIES true "${KEEPSAKE_CORE_DIR}/*")
set(KEEPSAKE_CORE_INCLUDES "")
foreach(component ${KEEPSAKE_CORE_COMPONENTS})
    if(IS_DIRECTORY "${component}")
        if(IS_DIRECTORY "${component}/include")
            list(APPEND KEEPSAKE_CORE_INCLUDES "${component}/include")
        endif()
        # Component-private headers (e.g. epaper_ui/render_utils.h) live next to the sources;
        # tests may exercise them directly.
        list(APPEND KEEPSAKE_CORE_INCLUDES "${component}")
    endif()
endforeach()

set(KEEPSAKE_WARNINGS -Wall -Wextra)
set(KEEPSAKE_SANITIZER_FLAGS "")
if(KEEPSAKE_SANITIZE)
    set(KEEPSAKE_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer
                                 -fno-sanitize-recover=undefined)
endif()

add_library(keepsake_core STATIC ${KEEPSAKE_CORE_SOURCES})
target_include_directories(keepsake_core PUBLIC ${KEEPSAKE_CORE_INCLUDES})
target_compile_options(keepsake_core PRIVATE ${KEEPSAKE_WARNINGS} ${KEEPSAKE_SANITIZER_FLAGS})
target_link_options(keepsake_core PUBLIC ${KEEPSAKE_SANITIZER_FLAGS})

# doctest (MIT), fetched at configure time and pinned to a release tag.
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

file(GLOB KEEPSAKE_UNIT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/unit/*.cpp")
add_executable(unit_tests ${KEEPSAKE_UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE keepsake_core doctest::doctest)
target_compile_options(unit_tests PRIVATE ${KEEPSAKE_WARNINGS} ${KEEPSAKE_SANITIZER_FLAGS})

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)

if(KEEPSAKE_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "KEEPSAKE_FUZZ needs clang (configure with -DCMAKE_CXX_COMPILER=clang++)")
    endif()
    target_compile_options(keepsake_core PRIVATE -fsanitize=fuzzer-no-link)
    file(GLOB KEEPSAKE_FUZZ_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/fuzz/fuzz_*.cpp")
    foreach(fuzz_source ${KEEPSAKE_FUZZ_SOURCES})
        get_filename_component(fuzz_name "${fuzz_source}" NAME_WE)
        add_executable(${fuzz_name} "${fuzz_source}")
        target_link_libraries(${fuzz_name} PRIVATE keepsake_core)
        target_compile_options(${fuzz_name} PRIVATE ${KEEPSAKE_WARNINGS} -fsanitize=fuzzer
                                                    ${KEEPSAKE_SANITIZER_FLAGS})
        target_link_options(${fuzz_name} PRIVATE -fsanitize=fuzzer)
    endforeach()
endif()
