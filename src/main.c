#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define STACK_KEY_FILE_NAME "stack_key"
#define GATE_KEY_FILE_NAME "gate_key"

#define PROCESS_NAME "Main"
#define PROCESS_COLOR COLOR_BLUE

#define GATE_SEMAPHORE_STARTING_VALUE 2

#define STARTING_DRONE_COUNT_DEFAULT 4
#define RESUPPLY_INTERVAL_DEFAULT 1000000
#define MAXIMUM_CHARGE_TIME_DEFAULT 4000000
#define MAXIMUM_LOADING_CYCLES 3

#define MAXIMUM_DRONES_IN_MEMORY 10

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
    print_msg("maximum_drones_count = %d", p_configuration->maximum_drones_count);
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

int create_shm_config(SHM_Configuration **out_cfg, int *out_sem_id) {
    if (!out_cfg || !out_sem_id) return -1;

    key_t key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    int shm_id = shm_create(key, sizeof(SHM_Configuration));
    if (shm_id == -1) {
        print_error("Cant create shm");
        return -1;
    }

    SHM_Configuration *cfg = shm_attach(shm_id);
    if (!cfg) {
        print_error("Cant attach shm");
        return -1;
    }

    *cfg = (SHM_Configuration){
        .next_dron_id = 0,
        .starting_drones_count = STARTING_DRONE_COUNT_DEFAULT,
        .resupply_interval = RESUPPLY_INTERVAL_DEFAULT,
        .maximum_charge_time = MAXIMUM_CHARGE_TIME_DEFAULT,
        .max_loading_cycles = MAXIMUM_LOADING_CYCLES,
    };

    *out_cfg = cfg;
    *out_sem_id = semaphore_create(key, 1);
    if (*out_sem_id == -1) {
        print_error("Cant create semaphore");
        return -1;
    }

    return shm_id;
}

int create_shm_all_drones_data(SHM_AllDronesData **out_data, Stack **out_stack, int *out_stack_id, int *out_sem_id) {
    if (!out_data || !out_sem_id) return -1;

    key_t shm_all_drones_data_key = grab_key_from_file(ALL_DRONES_DATA_FILE_NAME);
    if (shm_all_drones_data_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    size_t bytes_needed = sizeof(SHM_AllDronesData) + MAXIMUM_DRONES_IN_MEMORY * sizeof(DronData);
    const int shm_all_drones_data_id = shm_create(shm_all_drones_data_key, bytes_needed);

    SHM_AllDronesData *p_shm_all_drones_data = shm_attach(shm_all_drones_data_id);
    *p_shm_all_drones_data = (SHM_AllDronesData){
        .dron_in_base_count = 0,
        .drone_lost_count = 0,
        .dron_count = 0
    };

    bytes_needed = Stack_bytes_needed(MAXIMUM_DRONES_IN_MEMORY, sizeof(int));

    const key_t shm_stack_key = grab_key_from_file(STACK_KEY_FILE_NAME);
    if (shm_stack_key < 0) {
        print_error("Cant grab key");
    }
    const int shm_stack_id = shm_create(shm_stack_key, bytes_needed);
    Stack *p_shm_stack = shm_attach(shm_stack_id);
    Stack_init(p_shm_stack, MAXIMUM_DRONES_IN_MEMORY, sizeof(int));
    if (!p_shm_stack) {
        print_error("Stack Failed with initialization");
        return -1;
    }

    int index = MAXIMUM_DRONES_IN_MEMORY - 1;
    while (!Stack_is_full(p_shm_stack)) {
        Stack_push(p_shm_stack, &index);
        index--;
    }

    *out_sem_id = semaphore_create(shm_all_drones_data_key, 1);
    *out_data = p_shm_all_drones_data;
    *out_stack = p_shm_stack;
    *out_stack_id = shm_stack_id;

    return shm_all_drones_data_id;
}

int create_gate_semaphore() {
    const key_t gate_key = grab_key_from_file(GATE_KEY_FILE_NAME);
    if (gate_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int gate_semaphore_id = semaphore_create(gate_key, GATE_SEMAPHORE_STARTING_VALUE);
    return gate_semaphore_id;
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    signal(SIGINT, SIG_IGN);


    //Setting up shm for config
    SHM_Configuration *shm_configuration = NULL;
    int shm_config_semaphore_id = -1;

    const int shm_configuration_id = create_shm_config(&shm_configuration, &shm_config_semaphore_id);
    if (shm_configuration_id == EXIT_FAILURE) {
        // Todo handle error
    }
    process_argv(shm_configuration, argc, argv);
    print_configuration(shm_configuration);

    //Setting up shm for dron_info
    SHM_AllDronesData *shm_all_drones_data = NULL;
    Stack *shm_stack = NULL;
    int shm_stack_id = -1;
    int shm_all_drones_data_semaphore_id = -1;

    const int shm_all_drones_data_id = create_shm_all_drones_data(&shm_all_drones_data,
                                                                  &shm_stack,
                                                                  &shm_stack_id,
                                                                  &shm_all_drones_data_semaphore_id);
    if (shm_all_drones_data_id == EXIT_FAILURE) {
        // Todo handle error
    }


    const int gate_semaphore_id = create_gate_semaphore();
    if (gate_semaphore_id == EXIT_FAILURE) {
        // Todo handle error
    }


    const int operator_pid = creat_operator();
    if ((operator_pid) < 0) {
        // Todo handle error
    }

    // const int system_commander_pid = creat_system_commander();
    // if (system_commander_pid < 0) {
    //     // Todo handle error
    // }

    print_msg("Waiting for children...");

    // waitpid(system_commander_pid, NULL, 0);
    // print_msg("System_commander joined");

    waitpid(operator_pid, NULL, 0);
    print_msg("Operator joined");


    shm_detach(shm_configuration);
    shm_destroy(shm_configuration_id);

    shm_detach(shm_all_drones_data);
    shm_destroy(shm_all_drones_data_id);

    shm_detach(shm_stack);
    shm_destroy(shm_stack_id);

    semaphore_delete(shm_config_semaphore_id);
    semaphore_delete(shm_all_drones_data_semaphore_id);
    semaphore_delete(gate_semaphore_id);

    print_msg("Cleanup complete.");
    return 0;
}
