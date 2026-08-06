cmake_minimum_required(VERSION 3.20)

project(concurrent_matching_engine)

set(CMAKE_CXX_STANDARD 20)

option(ENABLE_ASAN "Enable AddressSanitizer" ON)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" ON)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

add_library(order_book_lib
    src/order_book.cpp
)

target_include_directories(order_book_lib
    PUBLIC include
)

add_executable(order_book_tests
    tests/test_matching.cpp
)

target_link_libraries(order_book_tests
    PRIVATE order_book_lib
)

target_include_directories(order_book_tests
    PRIVATE third_party
)

foreach(target order_book_lib order_book_tests)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wold-style-cast
        -Wnon-virtual-dtor
    )
endforeach()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")

    if(ENABLE_TSAN)

        foreach(target order_book_lib order_book_tests)
            target_compile_options(${target} PRIVATE
                -fsanitize=thread
            )

            target_link_options(${target} PRIVATE
                -fsanitize=thread
            )
        endforeach()

    else()

        if(ENABLE_ASAN)
            foreach(target order_book_lib order_book_tests)
                target_compile_options(${target} PRIVATE
                    -fsanitize=address
                )

                target_link_options(${target} PRIVATE
                    -fsanitize=address
                )
            endforeach()
        endif()

        if(ENABLE_UBSAN)
            foreach(target order_book_lib order_book_tests)
                target_compile_options(${target} PRIVATE
                    -fsanitize=undefined
                )

                target_link_options(${target} PRIVATE
                    -fsanitize=undefined
                )
            endforeach()
        endif()

    endif()

endif()

find_program(CLANG_TIDY clang-tidy)

if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

if(UNIX)
    target_link_libraries(order_book_tests PRIVATE pthread)
endif()

enable_testing()

add_test(NAME order_book_tests COMMAND order_book_tests)