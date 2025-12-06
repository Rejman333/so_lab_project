cmake_minimum_required(VERSION 3.25)
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

# === semaphore library ===
add_library(semaphore
        ${SRC_DIR}/ipc/semaphore.c
)


target_include_directories(semaphore PUBLIC
        ${SRC_DIR}/ipc
        ${SRC_DIR}/utils     # <-- kluczowe!
)

# oraz musi linkować printer, bo korzysta z print_error()
target_link_libraries(semaphore
        PRIVATE printer
)

# === main ===
add_executable(main_program
        ${SRC_DIR}/main.c
)

target_link_libraries(main_program
        printer
        semaphore
)

# === dron ===
add_executable(dron
        ${SRC_DIR}/dron/dron.c
)

target_link_libraries(dron
        printer
        semaphore
)

# === operator ===
add_executable(operator
        ${SRC_DIR}/operator/operator.c
)

target_link_libraries(operator
        printer
        semaphore
)

# === system commander ===
add_executable(system_commander
        ${SRC_DIR}/system_commander/system_commander.c
)

target_link_libraries(system_commander
        printer
        semaphore   # jeśli korzysta z semaforów
)
