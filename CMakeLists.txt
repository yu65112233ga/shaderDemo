cmake_minimum_required(VERSION 3.10)

project(shaderDemo)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add ANGLE include directories
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/angle/include)

# Add stb include directory
include_directories(${CMAKE_SOURCE_DIR}/thirdparty)

# Include project directories
include_directories(${CMAKE_SOURCE_DIR})

# Add ANGLE library directories
link_directories(${CMAKE_SOURCE_DIR}/thirdparty/angle/libs)

# Source files
set(SOURCES
    main.cpp
    reader/ImageLoader.cpp
    render/Renderer.cpp
    computeRenderer/Renderer.cpp
)

# Add executable
add_executable(shaderDemo ${SOURCES})

# Link ANGLE libraries
target_link_libraries(shaderDemo
    ${CMAKE_SOURCE_DIR}/thirdparty/angle/libs/libGLESv2.dll.lib
    ${CMAKE_SOURCE_DIR}/thirdparty/angle/libs/libEGL.dll.lib
)
