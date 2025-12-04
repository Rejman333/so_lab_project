cmake_minimum_required(VERSION 3.25)
project(so_drone_swarm C)

set(CMAKE_C_STANDARD 11)

# folder źródłowy
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# --- printer library ---
add_library(printer
        ${SRC_DIR}/utils/printer.c
)

# udostępnij katalog z nagłówkami biblioteki
target_include_directories(printer PUBLIC
        ${SRC_DIR}/utils
)

# --- main ---
add_executable(main_program
        ${SRC_DIR}/main.c
)

# linkujemy bibliotekę printer do main
target_link_libraries(main_program
        printer
)

# --- dron ---
add_executable(dron
        ${SRC_DIR}/dron/dron.c
)

target_link_libraries(dron
        printer
)

# --- operator ---
add_executable(operator
        ${SRC_DIR}/operator/operator.c
)

target_link_libraries(operator
        printer
)

# --- system commander ---
add_executable(system_commander
        ${SRC_DIR}/system_commander/system_commander.c
)

target_link_libraries(system_commander
        printer
)
