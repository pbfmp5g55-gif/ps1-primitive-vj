cmake_minimum_required(VERSION 3.16)
project(ps1_primitive_vj CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(vj STATIC
    vj/src/MidiController.cpp
    vj/src/RandomController.cpp
    vj/src/SafetyLimiter.cpp
    vj/src/DepthDelayQueue.cpp
    vj/src/PrimitiveContext.cpp
    vj/src/PrimitiveInterceptor.cpp
    vj/src/DebugOverlay.cpp
    vj/src/AutoMode.cpp
)
target_include_directories(vj PUBLIC vj/include)

option(VJ_ENABLE_RTMIDI "Build RtMidi-backed MidiController (host-side use; pulls RtMidi 6.0.0 via FetchContent)" OFF)
if(VJ_ENABLE_RTMIDI)
    include(FetchContent)
    set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(RTMIDI_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        rtmidi
        GIT_REPOSITORY https://github.com/thestk/rtmidi.git
        GIT_TAG        6.0.0
    )
    FetchContent_MakeAvailable(rtmidi)
    target_sources(vj PRIVATE vj/src/RtMidiController.cpp)
    target_link_libraries(vj PUBLIC rtmidi)
    target_compile_definitions(vj PUBLIC VJ_HAS_RTMIDI=1)
endif()

option(VJ_BUILD_DEMO "Build the standalone demo executable" ON)
if(VJ_BUILD_DEMO)
    add_executable(vj_demo vj/example/demo.cpp)
    target_link_libraries(vj_demo PRIVATE vj)
endif()

option(VJ_BUILD_TESTS "Build unit tests" ON)
if(VJ_BUILD_TESTS)
    enable_testing()
    set(VJ_TEST_SOURCES
        vj/tests/test_depth_delay_queue.cpp
        vj/tests/test_safety_limiter.cpp
        vj/tests/test_random_controller.cpp
        vj/tests/test_primitive_interceptor_filter.cpp
        vj/tests/test_auto_mode.cpp
    )
    foreach(test_src ${VJ_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE vj)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
