#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define STACK_KEY_FILE_NAME "stack_key"

#define PROCESS_NAME "System Commander"
#define PROCESS_COLOR COLOR_ORANGE

static volatile sig_atomic_t got_shutdown_requested = 0;

void shutdown_request_handler(int sig) {
    got_shutdown_requested = 1;
}

int get_shm_all_drones_data(SHM_AllDronesData **out_data, Stack **out_stack, int *out_stack_id, int *out_sem_id) {
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

int send_add_drones(const pid_t operator_pid) {
    print_msg("Sending add_drones to operator on pid: %d", operator_pid);
    if (kill(operator_pid,SIGUSR1) < 0) return -1;
    return 0;
}

int send_subtract_drones(const pid_t operator_pid) {
    print_msg("Sending subtract_drones to operator on pid: %d", operator_pid);
    if (kill(operator_pid,SIGUSR2) < 0) return -1;
    return 0;
}

int send_suicide(const pid_t dron_pid) {
    print_msg("Sending suicide to dron on pid: %d", dron_pid);
    if (kill(dron_pid,SIGUSR1) < 0) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    struct sigaction sig_shutdown_request;
    sig_shutdown_request.sa_handler = shutdown_request_handler;
    sigfillset(&sig_shutdown_request.sa_mask);
    sig_shutdown_request.sa_flags = 0;
    sigaction(SIGINT, &sig_shutdown_request, NULL);
    sigaction(SIGTERM, &sig_shutdown_request, NULL);

    if (argc < 2) {
        print_error("Operator pid not specified");
        exit(-1);
    }

    const int operator_pid = atoi(argv[1]);
    if (operator_pid < 0) {
        print_error("Operator pid cant be < 0");
        exit(-1);
    }


    SHM_AllDronesData *shm_all_drones_data = NULL;
    Stack *shm_stack = NULL;
    int shm_stack_id = -1;
    int shm_all_drones_data_semaphore_id = -1;

    const int shm_all_drones_data_id = get_shm_all_drones_data(&shm_all_drones_data,
                                                               &shm_stack,
                                                               &shm_stack_id,
                                                               &shm_all_drones_data_semaphore_id);
    if (shm_all_drones_data_id == EXIT_FAILURE) {
        // Todo handle error
    }


    //Mission Plan
    print_msg("Simulation Plan: Starting");
    print_msg("Simulation Plan: Increasing Drones");
    for (int i = 0; i < 3; ++i) {
        if (got_shutdown_requested) exit(0);
        sleep(5);
        if (send_add_drones(operator_pid) == -1) {
            print_error("Sending signal failed");
        }
    }

    for (int i = 0; i < 10; ++i) {
        if (got_shutdown_requested) exit(0);
        sleep(5);

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }
        const int dron_pid = SHM_AllDronesData_get_dron_pid(shm_all_drones_data);
        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        if (dron_pid < 0 ) {
            print_msg("Failed to find dron in shm");
            continue;
        };
        if (send_suicide(dron_pid) == -1) {
            print_error("Sending signal failed");
        }
    }

    print_msg("Simulation Plan: Reducing Drones");
    for (int i = 0; i < 10; ++i) {
        if (got_shutdown_requested) exit(0);
        sleep(5);
        if (send_subtract_drones(operator_pid) == -1) {
            print_error("Sending signal failed");
        }
    }

    print_msg("Simulation Plan: Finished");
    return 0;
}
