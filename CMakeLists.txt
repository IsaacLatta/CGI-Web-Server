cmake_minimum_required(VERSION 3.10)

project(WebServer VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories for headers and ASIO
include_directories(include)
include_directories(third_party/ASIO)

# Define ASIO_STANDALONE for standalone Asio usage
add_definitions(-DASIO_STANDALONE)

# Collect source files
file(GLOB SOURCES "src/*.cpp")

# Find OpenSSL
find_package(OpenSSL REQUIRED)
if (NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found, make sure it is installed.")
endif()

# Define the executable target
add_executable(WebServer ${SOURCES})

# Link with OpenSSL libraries
target_link_libraries(WebServer OpenSSL::SSL OpenSSL::Crypto)

# Compiler warnings
target_compile_options(WebServer PRIVATE -Wall -Wextra -pedantic)

# Set output directory
set_target_properties(WebServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Optional installation target
install(TARGETS WebServer DESTINATION /usr/local/bin)
