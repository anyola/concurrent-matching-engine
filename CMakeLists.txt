cmake_minimum_required(VERSION 3.20)

project(concurrent_matching_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)

FetchContent_MakeAvailable(nlohmann_json)

find_package(Boost REQUIRED COMPONENTS system)

include(CTest)

add_executable(server
    src/main.cpp
    src/order_book.cpp
    src/server/tcp_server.cpp
    src/server/session.cpp
    src/server/protocol.cpp
)

target_include_directories(server PRIVATE
    include
)

target_link_libraries(server PRIVATE
    Boost::system
    nlohmann_json::nlohmann_json
)

add_executable(server_tests
    tests/server_tests.cpp
    src/order_book.cpp
    src/server/tcp_server.cpp
    src/server/session.cpp
    src/server/protocol.cpp
)

target_include_directories(server_tests PRIVATE
    include
    third_party
)

target_link_libraries(server_tests PRIVATE
    Boost::system
    nlohmann_json::nlohmann_json
)

add_test(NAME server_tests COMMAND server_tests)