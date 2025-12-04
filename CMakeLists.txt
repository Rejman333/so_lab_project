cmake_minimum_required(VERSION 3.25)
project(so_drone_swarm C)

set(CMAKE_C_STANDARD 11)

# folder źródłowy
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# --- main ---
add_executable(main_program
        ${SRC_DIR}/main.c
)

# --- dron ---
add_executable(dron_program
        ${SRC_DIR}/dron/dron.c
)

# --- operator ---
add_executable(operator_program
        ${SRC_DIR}/operator/operator.c
)

# --- system commander ---
add_executable(system_commander_program
        ${SRC_DIR}/system_commander/system_commander.c
)
