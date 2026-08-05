cmake_minimum_required(VERSION 3.20)

project(concurrent_matching_engine)

set(CMAKE_CXX_STANDARD 20)

add_library(order_book_lib
    src/order_book.cpp
)

target_include_directories(order_book_lib PUBLIC include)

add_executable(order_book_tests
    tests/test_matching.cpp
)

target_link_libraries(order_book_tests PRIVATE order_book_lib)

target_include_directories(order_book_tests PRIVATE third_party)

enable_testing()
add_test(NAME order_book_tests COMMAND order_book_tests)