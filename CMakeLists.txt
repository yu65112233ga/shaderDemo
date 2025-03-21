cmake_minimum_required(VERSION 3.10)

project(shaderDemo)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add ANGLE include directories
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/angle/include)

# Add ANGLE library directories
link_directories(${CMAKE_SOURCE_DIR}/thirdparty/angle/libs)

# Add executable
add_executable(shaderDemo main.cpp)

# Link ANGLE libraries
target_link_libraries(shaderDemo
    ${CMAKE_SOURCE_DIR}/thirdparty/angle/libs/libGLESv2.dll.lib
    ${CMAKE_SOURCE_DIR}/thirdparty/angle/libs/libEGL.dll.lib
)
