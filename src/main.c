#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#include "ipc.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define DRON_INFO_KEY_FILE_NAME "dron_info_key"
#define GATE_KEY_FILE_NAME "gate_key"

#define PROCESS_NAME "Main"
#define PROCESS_COLOR COLOR_BLUE

#define GATE_SEMAPHORE_STARTING_VALUE 2

#define STARTING_DRONE_COUNT_DEFAULT 10
#define RESUPPLY_INTERVAL_DEFAULT 1000000
#define MAXIMUM_CHARGE_TIME_DEFAULT 10000000
#define MAXIMUM_LOADING_CYCLES 5

void process_argv(SHM_Configuration *p_configuration, int argc, char *argv[]) {
    if (argc > 1) {
        p_configuration->starting_drones_count = atoi(argv[1]);
        if (p_configuration->starting_drones_count <= 0) {
            print_error("starting_drones_count must be > 0");
            exit(1);
        }
    }

    if (argc > 2) {
        p_configuration->resupply_interval = atoi(argv[2]);
        if (p_configuration->resupply_interval <= 0) {
            print_error("resupply_interval must be > 0");
            exit(1);
        }
    }

    if (argc > 3) {
        p_configuration->maximum_charge_time = atoi(argv[3]);
        if (p_configuration->maximum_charge_time <= 0) {
            print_error("maximum_charge_time must be > 0");
            exit(1);
        }
    }

    if (argc > 4) {
        p_configuration->max_loading_cycles = atoi(argv[4]);
        if (p_configuration->max_loading_cycles <= 0) {
            print_error("max_loading_cycles must be > 0");
            exit(1);
        }
    }

    if (argc > 5) {
        print_error("Maximum argument count is 4");
        exit(1);
    }
}

void print_configuration(const SHM_Configuration *p_configuration) {
    print_msg("=== Starting Configuration ===");
    print_msg("starting_drones_count = %d", p_configuration->starting_drones_count);
    print_msg("resupply_interval = %d", p_configuration->resupply_interval);
    print_msg("maximum_charge_time = %d", p_configuration->maximum_charge_time);
    print_msg("max_loading_cycles = %d", p_configuration->max_loading_cycles);
}

int creat_operator() {
    int pid = fork();
    if (pid == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);

        execl("./operator", "./operator", NULL);
        perror("exec operator");
        exit(1);
    }
    return pid;
}

int creat_system_commander() {
    int pid = fork();
    if (pid == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);

        execl("./system_commander", "./system_commander", NULL);
        perror("exec system_commander");
        exit(1);
    }
    return pid;
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    signal(SIGINT, SIG_IGN);

    key_t shm_config_key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (shm_config_key < 0) {
        print_error("Cant grab key");
    }
    int shm_config_id = shm_create(shm_config_key, sizeof(SHM_Configuration));
    SHM_Configuration *p_shm_config = shm_attach(shm_config_id);
    *p_shm_config = (SHM_Configuration){
        .starting_drones_count = STARTING_DRONE_COUNT_DEFAULT,
        .resupply_interval = RESUPPLY_INTERVAL_DEFAULT,
        .maximum_charge_time = MAXIMUM_CHARGE_TIME_DEFAULT,
        .max_loading_cycles = MAXIMUM_LOADING_CYCLES
    };

    process_argv(p_shm_config, argc, argv);
    print_configuration(p_shm_config);

    key_t shm_dron_info_key = grab_key_from_file(DRON_INFO_KEY_FILE_NAME);
    if (shm_dron_info_key < 0) {
        print_error("Cant grab key");
    }

    int shm_dron_info_id = shm_create(shm_dron_info_key,
        sizeof(SHM_DronInfo) + p_shm_config->starting_drones_count * 2 * sizeof(Dron_State));
    SHM_DronInfo *p_shm_dron_info = shm_attach(shm_dron_info_id);
    *p_shm_dron_info = (SHM_DronInfo){
        .dron_in_base_count = 0,
        .drone_lost_count = 0,
        .missions_completed_count = 0,
        .dron_count = 0
    };

    // p_shm_dron_info->dron_state_array[0] = (Dron_State){
    //     .dron_location = LOCATION_UNDEFINE,
    //     .loading_cycles_left = 10,
    //     .pid = 333,
    //     .last_update = time(NULL)
    // };

    int shm_config_semaphore_id = create_semaphore(shm_config_key, 1);
    int shm_dron_info_semaphore_id = create_semaphore(shm_dron_info_key, 1);

    key_t gate_key = grab_key_from_file(GATE_KEY_FILE_NAME);
    if (gate_key < 0) {
        print_error("Cant grab key");
    }

    int gate_semaphore_id = create_semaphore(gate_key, GATE_SEMAPHORE_STARTING_VALUE);



    int operator_pid;
    if ((operator_pid = creat_operator()) < 0) {
        print_error("Operator process failed to start");
    }

    // int system_commander_pid;
    // if ((system_commander_pid = creat_system_commander()) < 0) {
    //     print_error("System commander process failed to start");
    // }

    print_msg("Waiting for children...");

    // waitpid(system_commander_pid, NULL, 0);
    // print_msg("System_commander joined");

    waitpid(operator_pid, NULL, 0);
    print_msg("Operator joined");


    shm_detach(p_shm_config);
    shm_destroy(shm_config_id);

    shm_detach(p_shm_dron_info);
    shm_destroy(shm_dron_info_id);

    delete_semaphore(shm_config_semaphore_id);
    delete_semaphore(shm_dron_info_semaphore_id);
    delete_semaphore(gate_semaphore_id);

    print_msg("Cleanup complete.");
    return 0;
}
