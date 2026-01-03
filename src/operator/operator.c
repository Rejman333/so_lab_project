#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>

#include "ipc.h"
#include "printer.h"


#define CONFIG_KEY_FILE_NAME "config_key"
#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define STACK_KEY_FILE_NAME "stack_key"

#define PROCESS_NAME "Operator"
#define PROCESS_COLOR COLOR_CYAN

#define LOG_FILE_NAME "log.txt"

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigusr2 = 0;
static volatile sig_atomic_t got_shutdown_requested = 0;

SHM_Configuration local_configuration = {0};

SHM_AllDronesData *shm_all_drones_data = NULL;
int shm_all_drones_data_id = -1;
Stack *shm_stack = NULL;
int shm_stack_id = -1;
int shm_all_drones_data_semaphore_id = -1;


void shutdown_request_handler(int sig) {
    got_shutdown_requested = 1;
}

void sigusr1_handler(int sig) {
    got_sigusr1 = 1;
}

void sigusr2_handler(int sig) {
    got_sigusr2 = 1;
}

//Todo Redo
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
        exit(1);
    }
    return dron_pid;
}

int generate_starting_drones() {
    print_msg("=== Commander: Generating %d starting drones ===", local_configuration.starting_drones_count);
    for (int i = 0; i < local_configuration.starting_drones_count; ++i) {
        print_msg("Creating dron");
        if (creat_dron(LOCATION_MISSION) < 0) {
            print_error("Failed to creat starting drone");
            return -1;
        };
    }
    return 0;
}

int describe_self() {
    if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    print_msg("Drones in base: [%d+%d|%d] | Drones on missions [%d]",
              shm_all_drones_data->dron_in_base_count,
              shm_all_drones_data->dron_reserving_space_count,
              shm_all_drones_data->maximum_dron_in_base_count,
              shm_all_drones_data->dron_count - shm_all_drones_data->dron_in_base_count);

    if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    return 0;
}

int close_main(const int exit_code) {
    if (shm_all_drones_data_id > -1 && shm_all_drones_data) {
        if (shm_detach(shm_all_drones_data) != 0) {
            print_error("shm_detach all_drones_data failed");
        }

        shm_all_drones_data = NULL;
        shm_all_drones_data_id = -1;
    }
    if (shm_stack_id > -1 && shm_stack) {
        if (shm_detach(shm_stack) != 0) {
            print_error("shm_detach stack failed");
        }

        shm_stack = NULL;
        shm_stack_id = -1;
    }
    if (shm_all_drones_data_semaphore_id > -1) shm_all_drones_data_semaphore_id = -1;

    int status;
    while (wait(&status) > 0) {
    };


    logger_shutdown();
    print_msg("Closing operator process");
    exit(exit_code);
}

int get_configuration_from_shm_config() {
    const key_t key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (key < 0) {
        print_error("Cant grab key from file: %s", CONFIG_KEY_FILE_NAME);
        return -1;
    }

    const int shm_configuration_id = shm_get(key);
    if (shm_configuration_id == -1) {
        print_error("Cant get shm for configuration");
        return -1;
    }


    const int shm_configuration_semaphore_id = semaphore_get(key);
    if (shm_configuration_semaphore_id == -1) {
        print_error("Cant get semaphore for configuration");
        return -1;
    }

    SHM_Configuration *shm_configuration = shm_attach(shm_configuration_id);
    if (!shm_configuration) {
        print_error("Cant attach shm");
        return -1;
    }

    if (semaphore_lock(shm_configuration_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        if (shm_detach(shm_configuration) != 0) {
            print_error("shm_detach configuration failed");
            return -1;
        }
        return -1;
    }

    local_configuration = *shm_configuration;

    if (semaphore_unlock(shm_configuration_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        if (shm_detach(shm_configuration) != 0) {
            print_error("shm_detach configuration failed");
            return -1;
        }
        return -1;
    }

    if (shm_detach(shm_configuration) != 0) {
        print_error("shm_detach configuration failed");
        return -1;
    }
    return 0;
}

int get_shm_all_drones_data() {
    const key_t shm_all_drones_data_key = grab_key_from_file(ALL_DRONES_DATA_FILE_NAME);
    if (shm_all_drones_data_key < 0) {
        print_error("Cant grab key");
        return -1;
    }


    shm_all_drones_data_id = shm_get(shm_all_drones_data_key);
    if (shm_all_drones_data_id < 0) {
        print_error("Failed to get shm for all_drones_data");
        return -1;
    }

    shm_all_drones_data = shm_attach(shm_all_drones_data_id);
    if (shm_all_drones_data == NULL) {
        print_error("Failed to attach shm for all_drones_data");
        return -1;
    }

    const key_t shm_stack_key = grab_key_from_file(STACK_KEY_FILE_NAME);
    if (shm_stack_key < 0) {
        print_error("Cant grab key");
    }
    shm_stack_id = shm_get(shm_stack_key);
    if (shm_stack_id < 0) {
        print_error("Failed to get shm for stack");
        return -1;
    }
    shm_stack = shm_attach(shm_stack_id);
    if (shm_stack == NULL) {
        print_error("Failed to attach shm for stack");
        return -1;
    }

    shm_all_drones_data_semaphore_id = semaphore_get(shm_all_drones_data_key);
    if (shm_all_drones_data_semaphore_id < 0) {
        print_error("Cant get semaphore for all_drones_data");
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    if (logger_initialize(LOG_FILE_NAME) != 0) {
        print_error("Logger initialization failed");
        close_main(EXIT_FAILURE);
    }

    struct sigaction sa_shutdown = {0};
    sa_shutdown.sa_handler = shutdown_request_handler;

    sigfillset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa_shutdown, NULL) == -1) {
        print_error("sigaction(SIGINT) failed");
        close_main(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa_shutdown, NULL) == -1) {
        print_error("sigaction(SIGTERM) failed");
        close_main(EXIT_FAILURE);
    }

    struct sigaction sa_usr1 = {0};
    sa_usr1.sa_handler = sigusr1_handler;

    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        print_error("sigaction(SIGUSR1) failed");
        close_main(EXIT_FAILURE);
    }

    struct sigaction sa_usr2 = {0};
    sa_usr2.sa_handler = sigusr2_handler;

    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR2, &sa_usr2, NULL) == -1) {
        print_error("sigaction(SIGUSR2) failed");
        close_main(EXIT_FAILURE);
    }

    if (get_configuration_from_shm_config() != 0) {
        print_error("Failed to get configuration from shm");
        close_main(EXIT_FAILURE);
    }

    if (get_shm_all_drones_data() != 0) {
        print_error("Failed to get/attach shm/semaphore");
        close_main(EXIT_FAILURE);
    }

    if (generate_starting_drones(local_configuration.starting_drones_count) != 0) {
        print_error("Generating drones failed");
        close_main(EXIT_FAILURE);
    }

    print_msg("=== Operator: Starting Fabrication ===");
    while (!got_shutdown_requested) {
        if (got_sigusr1) {
            got_sigusr1 = 0;

            if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                close_main(EXIT_FAILURE);
            }

            int new_max = shm_all_drones_data->maximum_dron_in_base_count * 2;
            if (new_max >= local_configuration.starting_drones_count * 2) {
                new_max = local_configuration.starting_drones_count * 2 - 1;
            }

            shm_all_drones_data->maximum_dron_in_base_count = new_max;

            if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                close_main(EXIT_FAILURE);
            }

            print_msg("Processed signal: sig_add_max_drones");
        }

        if (got_sigusr2) {
            got_sigusr2 = 0;

            if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                close_main(EXIT_FAILURE);
            }

            shm_all_drones_data->maximum_dron_in_base_count /= 2;
            if (shm_all_drones_data->maximum_dron_in_base_count <= 0) {
                got_shutdown_requested = 1;
            }

            if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                close_main(EXIT_FAILURE);
            }

            print_msg("Processed signal: sig_decrease_max_drones");
        }

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            close_main(EXIT_FAILURE);
        }

        if (shm_all_drones_data->dron_in_base_count + shm_all_drones_data->dron_reserving_space_count <
            shm_all_drones_data->maximum_dron_in_base_count) {
            if (creat_dron(LOCATION_BASE) < 0) {
                print_msg_color(COLOR_SKY_BLUE, "Failed to creat a drone");
                //We dont stop program on this
            }
        }

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            close_main(EXIT_FAILURE);
        }

        if (describe_self() != 0) {
            print_error("Describing self failed");
            close_main(EXIT_FAILURE);
        }

        usleep(local_configuration.resupply_interval);
    }

    close_main(EXIT_SUCCESS);
    return 0;
}
