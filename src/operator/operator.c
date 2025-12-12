#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define DRON_INFO_KEY_FILE_NAME "dron_info_key"

#define PROCESS_NAME "Operator"
#define PROCESS_COLOR COLOR_CYAN

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

int creat_dron(const Location location) {
    int dron_pid = fork();

    if (dron_pid == 0) {
        char location_arg[16];
        snprintf(location_arg, sizeof(location_arg), "%d", location);
        execl("./dron","./dron",location_arg, NULL);

        print_error("exec dron");
        _exit(1);
    }

    return dron_pid;
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    key_t shm_config_key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (shm_config_key < 0) {
        print_error("Cant grab key");
    }

    int shm_config_id = shm_open_existing(shm_config_key);
    if (shm_config_id < 0) {
        print_error("Cant open shm");
        _exit(1);
    }

    SHM_Configuration *p_shm_config = shm_attach(shm_config_id);
    int shm_config_semaphore_id = get_semaphore(shm_config_key);

    if (semop(shm_config_semaphore_id, &SEM_LOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        exit(1);
    }

    int starting_drones_count = p_shm_config->starting_drones_count;
    int resupply_interval = p_shm_config->resupply_interval;
    int target_number_of_drones = starting_drones_count;
    int max_drones_on_platform = (target_number_of_drones / 2) - 1;

    if (semop(shm_config_semaphore_id, &SEM_UNLOCK, 1) == -1) {
        print_error("While leaving semaphore: semop +1");
        exit(1);
    }

    shm_detach(p_shm_config);


    key_t shm_dron_info_key = grab_key_from_file(DRON_INFO_KEY_FILE_NAME);
    if (shm_dron_info_key < 0) {
        print_error("Cant grab key");
        _exit(1);
    }

    int shm_dron_info_id = shm_open_existing(shm_dron_info_key);
    if (shm_dron_info_id < 0) {
        print_error("Cant open shm");
        _exit(1);
    }

    SHM_DronInfo *p_shm_dron_info = shm_attach(shm_dron_info_id);
    int shm_dron_info_semaphore_id = get_semaphore(shm_dron_info_key);

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


    if (semop(shm_dron_info_semaphore_id, &SEM_LOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        exit(1);
    }

    for (int i = 0; i < starting_drones_count; ++i) {
        print_msg("Creating dron");
        p_shm_dron_info->dron_count++;
        if (creat_dron(LOCATION_MISSION) < 0) {
            p_shm_dron_info->dron_count--;
        };
    }

    if (semop(shm_dron_info_semaphore_id, &SEM_UNLOCK, 1) == -1) {
        print_error("While leaving semaphore: semop +1");
        exit(1);
    }

    while (!got_shutdown_requested) {
        if (got_sigusr1) {
            got_sigusr1 = 0;
            target_number_of_drones *= 2;
            if (target_number_of_drones > starting_drones_count * 2) {
                target_number_of_drones = starting_drones_count * 2;
            }
            max_drones_on_platform = (target_number_of_drones / 2) - 1;
            print_msg("Increased target_number_of_drones to %d", target_number_of_drones);
            print_msg("Increased max_drones_on_platform to %d", max_drones_on_platform);
        }

        if (got_sigusr2) {
            got_sigusr2 = 0;
            if (target_number_of_drones > 1) {
                target_number_of_drones /= 2;
                max_drones_on_platform = (target_number_of_drones / 2) - 1;
                print_msg("Decreased target_number_of_drones to %d", target_number_of_drones);
                print_msg("Decreased max_drones_on_platform to %d", max_drones_on_platform);
            }
        }

        if (semop(shm_dron_info_semaphore_id, &SEM_LOCK, 1) == -1) {
            print_error("While waiting for semaphore: semop -1");
            exit(1);
        }
        if (p_shm_dron_info->dron_in_base_count < max_drones_on_platform) {
            print_msg("Creating dron");
            if (creat_dron(LOCATION_BASE) < 0) {
                _exit(1);
            };
            p_shm_dron_info->dron_in_base_count++;
            p_shm_dron_info->dron_count++;
        }

        print_msg("Current dron count: %d", p_shm_dron_info->dron_count);

        if (semop(shm_dron_info_semaphore_id, &SEM_UNLOCK, 1) == -1) {
            print_error("While leaving semaphore: semop +1");
            exit(1);
        }

        usleep(resupply_interval);
    }

    print_msg("Closing operator process");
    return 0;
}
