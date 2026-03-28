find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE TESTKIT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/testkit/*.cc"
)

add_library(testkit STATIC ${TESTKIT_SOURCES})

target_include_directories(testkit
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/testkit"
)

target_link_libraries(testkit
        PUBLIC
        WebServer::core
        GTest::gtest
)

add_executable(core_tests
        unit/ResolverTests.cc
        unit/ServerTestsNoNetwork.cc
)

target_link_libraries(core_tests
        PRIVATE
        testkit
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_tests)