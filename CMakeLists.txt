cmake_minimum_required(VERSION 3.22)
project(NESRevolution VERSION 0.1.0 LANGUAGES CXX)

option(NESREV_BUILD_TESTS "Build portable core tests" ON)

add_subdirectory(core)
