cmake_minimum_required(VERSION 3.20)
project(esp32_cli_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- Unity test framework (downloaded automatically) ----
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

# ---- Code under test: the real cli component ----
set(CLI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components/cli)

add_library(cli STATIC
    ${CLI_DIR}/cli.c
    ${CLI_DIR}/cli_line.c
)
target_include_directories(cli PUBLIC ${CLI_DIR}/include)
target_compile_options(cli PRIVATE -Wall -Wextra -Werror)

# ---- Tests ----
enable_testing()

add_executable(test_cli test_cli.c fake_output.c)
target_link_libraries(test_cli PRIVATE cli unity)
add_test(NAME test_cli COMMAND test_cli)

add_executable(test_cli_line test_cli_line.c)
target_link_libraries(test_cli_line PRIVATE cli unity)
add_test(NAME test_cli_line COMMAND test_cli_line)

# ---- can_service (M6) ----
set(CAN_SERVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components/can_service)
set(PORTS_DIR       ${CMAKE_CURRENT_SOURCE_DIR}/../../components/ports)

add_library(can_service STATIC ${CAN_SERVICE_DIR}/can_service.c)
target_include_directories(can_service PUBLIC
    ${CAN_SERVICE_DIR}/include
    ${PORTS_DIR}/include
)
target_compile_options(can_service PRIVATE -Wall -Wextra -Werror)

add_executable(test_can_service test_can_service.c fake_can_port.c)
target_link_libraries(test_can_service PRIVATE can_service unity)
add_test(NAME test_can_service COMMAND test_can_service)
