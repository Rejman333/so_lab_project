cmake_minimum_required(VERSION 3.18.4)
project(so_drone_swarm C)

set(CMAKE_C_STANDARD 11)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

find_package(Threads REQUIRED)

add_library(stack
        ${SRC_DIR}/data_structures/stack.c
)

add_library(printer
        ${SRC_DIR}/utils/printer.c
)

target_include_directories(printer PUBLIC
        ${SRC_DIR}/utils
)


add_library(ipc
        ${SRC_DIR}/ipc/key.c
        ${SRC_DIR}/ipc/semaphore.c
        ${SRC_DIR}/ipc/shm.c
        ${SRC_DIR}/ipc/fifo.c

)

target_include_directories(ipc PUBLIC
        ${SRC_DIR}/ipc
        ${SRC_DIR}/utils
        ${SRC_DIR}/data_structures
)

target_link_libraries(ipc
        PRIVATE printer
        PRIVATE stack
)

# === main ===
add_executable(main_program
        ${SRC_DIR}/main.c
)

target_link_libraries(main_program
        printer
        ipc
)

# === dron ===
add_executable(dron
        ${SRC_DIR}/dron/dron.c
)

target_link_libraries(dron
        PRIVATE Threads::Threads
        printer
        ipc
)

# === operator ===
add_executable(operator
        ${SRC_DIR}/operator/operator.c
)

target_link_libraries(operator
        printer
        ipc
)

# === system commander ===
add_executable(system_commander
        ${SRC_DIR}/system_commander/system_commander.c
)

target_link_libraries(system_commander
        printer
        ipc
)

add_executable(test
        ${SRC_DIR}/test_stack.c
)

target_link_libraries(test
        printer
        stack
        ipc
)