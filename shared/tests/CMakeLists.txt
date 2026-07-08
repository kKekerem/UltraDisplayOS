include(GoogleTest)

add_executable(test_shared
    test_main.cpp
)

target_link_libraries(test_shared PRIVATE shared GTest::gtest GTest::gtest_main)

gtest_discover_tests(test_shared)
