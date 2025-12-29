#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define STACK_KEY_FILE_NAME "stack_key"

#define PROCESS_NAME "Operator"
#define PROCESS_COLOR COLOR_CYAN

typedef struct {
    int starting_drones_count;
    int max_drones_on_platform;
    int resupply_interval;
} OperatorConfiguration;

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigusr2 = 0;
static volatile sig_atomic_t got_shutdown_requested = 0;

void shutdown_request_handler(int sig) {
    got_shutdown_requested = 1;
}

void sigusr1_handler(int sig) {
    got_sigusr1 = 1;
}

void sigusr2_handler(int sig) {
    got_sigusr2 = 1;
}

int get_initial_configuration(OperatorConfiguration *out_config) {
    const key_t shm_config_key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (shm_config_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_config_id = shm_get(shm_config_key);
    if (shm_config_id < 0) {
        print_error("Cant open shm");
        return -1;
    }

    SHM_Configuration *p_shm_config = shm_attach(shm_config_id);
    const int shm_config_semaphore_id = semaphore_get(shm_config_key);

    if (semop(shm_config_semaphore_id, &SEM_LOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    out_config->starting_drones_count = p_shm_config->starting_drones_count;
    out_config->resupply_interval = p_shm_config->resupply_interval;

    if (semop(shm_config_semaphore_id, &SEM_UNLOCK, 1) == -1) {
        print_error("While leaving semaphore: semop +1");
        return -1;
    }
    shm_detach(p_shm_config);

    out_config->max_drones_on_platform = (out_config->starting_drones_count / 2) - 1;

    return 0;
}

int create_shm_all_drones_data(SHM_AllDronesData **out_data, Stack **out_stack, int *out_stack_id, int *out_sem_id) {
    if (!out_data || !out_sem_id) return -1;

    const key_t shm_all_drones_data_key = grab_key_from_file(ALL_DRONES_DATA_FILE_NAME);
    if (shm_all_drones_data_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_all_drones_data_id = shm_get(shm_all_drones_data_key);
    if (shm_all_drones_data_id < 0) {
        print_error("Cant open shm");
        return -1;
    }

    SHM_AllDronesData *p_shm_all_drones_data = shm_attach(shm_all_drones_data_id);

    const key_t shm_stack_key = grab_key_from_file(STACK_KEY_FILE_NAME);
    if (shm_stack_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_stack_id = shm_get(shm_stack_key);
    if (shm_stack_id < 0) {
        print_error("Cant open shm");
        return -1;
    }
    Stack *stack = shm_attach(shm_stack_id);


    *out_sem_id = semaphore_get(shm_all_drones_data_key);

    *out_data = p_shm_all_drones_data;
    *out_stack = stack;
    *out_stack_id = shm_stack_id;

    return shm_all_drones_data_id;
}

int creat_dron(const DronData_Location location) {
    const int dron_pid = fork();

    if (dron_pid == 0) {
        char slot_arg[16];
        snprintf(slot_arg, sizeof(slot_arg), "%d", location);

        execl(
            "./dron",
            "./dron",
            slot_arg,
            NULL
        );

        print_error("exec dron");
        _exit(1);
    }
    return dron_pid;
}

int generate_starting_drones(const int number_of_drones_to_create) {
    for (int i = 0; i < number_of_drones_to_create; ++i) {
        print_msg("Creating dron");
        if (creat_dron(LOCATION_MISSION) < 0) {
            print_error("Failed to creat a drone");
            return -1;
        };
    }
    return 0;
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    OperatorConfiguration local_configuration;

    if (get_initial_configuration(&local_configuration) == EXIT_FAILURE) {
        //Todo
    }

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


    struct sigaction sig_shutdown_request;
    sig_shutdown_request.sa_handler = shutdown_request_handler;
    sigfillset(&sig_shutdown_request.sa_mask);
    sig_shutdown_request.sa_flags = 0;
    sigaction(SIGINT, &sig_shutdown_request, NULL);
    sigaction(SIGTERM, &sig_shutdown_request, NULL);

    struct sigaction sig_add_max_drones;
    sig_add_max_drones.sa_handler = sigusr1_handler;
    sigemptyset(&sig_add_max_drones.sa_mask);
    sig_add_max_drones.sa_flags = 0;
    sigaction(SIGUSR1, &sig_add_max_drones, NULL);

    struct sigaction sig_decrease_max_drones_handler;
    sig_decrease_max_drones_handler.sa_handler = sigusr2_handler;
    sigemptyset(&sig_decrease_max_drones_handler.sa_mask);
    sig_decrease_max_drones_handler.sa_flags = 0;
    sigaction(SIGUSR2, &sig_decrease_max_drones_handler, NULL);

    print_msg("Generating %d starting drones.", local_configuration.starting_drones_count);
    generate_starting_drones(local_configuration.starting_drones_count);

    print_msg("Starting Fabrication");
    while (!got_shutdown_requested) {
        if (got_sigusr1) {
            got_sigusr1 = 0;
            local_configuration.max_drones_on_platform *= 2;
            if (local_configuration.max_drones_on_platform >= local_configuration.starting_drones_count * 2) {
                local_configuration.max_drones_on_platform = local_configuration.starting_drones_count * 2 - 1;
            }
            print_msg("Increased max_drones_on_platform to %d", local_configuration.max_drones_on_platform);
        }

        if (got_sigusr2) {
            got_sigusr2 = 0;
            local_configuration.max_drones_on_platform /= 2;
            print_msg("Decreased max_drones_on_platform to %d", local_configuration.max_drones_on_platform);
        }
    }


    print_msg("Closing operator process");
    return 0;
}
