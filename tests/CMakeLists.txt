# Tests live as a subdirectory of the main project — no separate project() needed.
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_sonifiq tst_sonifiq.cpp)

target_link_libraries(tst_sonifiq PRIVATE Qt6::Test)

# Run with offscreen platform so tests work in headless CI
add_test(
    NAME    SonifiQTests
    COMMAND tst_sonifiq
)
set_tests_properties(SonifiQTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
