cmake_minimum_required(VERSION 3.18.4)
project(so_drone_swarm C)

set(CMAKE_C_STANDARD 11)

# folder źródłowy
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)


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

)

target_include_directories(ipc PUBLIC
        ${SRC_DIR}/ipc
        ${SRC_DIR}/utils
)

target_link_libraries(ipc
        PRIVATE printer
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
