cmake_minimum_required(VERSION 3.10)

project(WebServer VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions(-DASIO_STANDALONE)

file(GLOB SOURCES "src/*.cpp" "third_party/tinyxml2/*.cpp")

# Find OpenSSL
find_package(OpenSSL REQUIRED)
if (NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found, make sure it is installed.")
endif()

add_executable(WebServer ${SOURCES})

target_include_directories(WebServer PRIVATE 
    include                        
    third_party/ASIO               # Asio library
    third_party/tinyxml2           # TinyXML2 library
    third_party/nlohmann           # nlohmann/json header-only library
)

# Link with OpenSSL libraries
target_link_libraries(WebServer OpenSSL::SSL OpenSSL::Crypto)

# Compiler warnings
target_compile_options(WebServer PRIVATE -Wall -Wextra -pedantic -Wno-sign-compare -Wno-unused-parameter)

# Set output directory
set_target_properties(WebServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Optional installation target
install(TARGETS WebServer DESTINATION /usr/local/bin)
