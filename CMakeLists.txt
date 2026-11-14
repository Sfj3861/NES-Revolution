cmake_minimum_required(VERSION 3.22)
project(NESRevolution VERSION 0.1.0 LANGUAGES CXX)

option(NESREV_BUILD_TESTS "Build portable core tests" ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(core)

if(ANDROID)
    add_library(nesrev_android SHARED android/native_bridge.cpp)
    target_compile_features(nesrev_android PRIVATE cxx_std_20)
    target_link_libraries(nesrev_android PRIVATE nesrev_core log)
endif()
