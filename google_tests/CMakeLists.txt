include(test_functions.cmake)

add_google_test(carry_basic test.cpp)
target_include_directories(carry_basic PRIVATE ${CMAKE_SOURCE_DIR}/include)