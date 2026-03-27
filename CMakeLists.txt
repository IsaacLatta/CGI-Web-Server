if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
            "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.20)

project(WebServer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)

find_package(tinyxml2 CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(core_warnings INTERFACE)

target_compile_options(core_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-unused-parameter>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
)
list(REMOVE_ITEM CORE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

add_library(core STATIC ${CORE_SOURCES})
add_library(WebServer::core ALIAS core)

target_include_directories(core
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(core
        PUBLIC
        asio::asio
        tinyxml2::tinyxml2
        nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp
        core_warnings
)

target_compile_definitions(core
        PUBLIC
        PROJECT_NAME="${PROJECT_NAME}"
        PROJECT_VERSION="${PROJECT_VERSION}"
        PROJECT_NAME_AND_VERSION="${PROJECT_NAME}_${PROJECT_VERSION}"
        $<$<CONFIG:Debug>:DBG_ENABLED=1>
        $<$<NOT:$<CONFIG:Debug>>:DBG_ENABLED=0>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:_GLIBCXX_ASSERTIONS>
)

add_executable(WebServer
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

target_link_libraries(WebServer
        PRIVATE
        core
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

