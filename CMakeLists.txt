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
)
target_include_directories(vj PUBLIC vj/include)

option(VJ_BUILD_DEMO "Build the standalone demo executable" ON)
if(VJ_BUILD_DEMO)
    add_executable(vj_demo vj/example/demo.cpp)
    target_link_libraries(vj_demo PRIVATE vj)
endif()
