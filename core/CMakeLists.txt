add_library(nesrev_core STATIC
    src/cartridge.cpp
    src/mapper0.cpp
    src/ppu2c02.cpp
    src/nes_machine.cpp
    src/controller.cpp
    src/input_queue.cpp
    src/cpu6502.cpp
)

target_include_directories(nesrev_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(nesrev_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(nesrev_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(nesrev_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(NESREV_BUILD_TESTS)
    enable_testing()
    add_executable(nesrev_core_tests tests/test_main.cpp)
    target_link_libraries(nesrev_core_tests PRIVATE nesrev_core)
    add_test(NAME nesrev_core_tests COMMAND nesrev_core_tests)
endif()
