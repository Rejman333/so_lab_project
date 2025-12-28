#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/sem.h>

#include "ipc.h"
#include "printer.h"

#define DRON_INFO_KEY_FILE_NAME "dron_info_key"
#define GATE_KEY_FILE_NAME "gate_key"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA


void sig_end_handler(int sig) {
    print_msg("Received SIGTERM, shutting down...");
    _exit(0);
}

void sig_suicide_handler(int sig) {
    print_msg("Received Suicide, crushing down...");
    _exit(0);
}

void *thread_charge_battery(void *arg) {
    return NULL;
}

int get_random_mission_time(int min, int max) {
    return min + rand() % (max - min + 1);
}


int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    srand(time(NULL));

    if (argc < 2) {
        write(STDERR_FILENO, "No slot argument\n", 17);
        _exit(1);
    }
    int my_slot = atoi(argv[1]);

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

    SHM_AllDronesData *p_shm_dron_info = shm_attach(shm_dron_info_id);
    int shm_dron_info_semaphore_id = get_semaphore(shm_dron_info_key);

    struct sigaction sig_end;
    sig_end.sa_handler = sig_end_handler;
    sigfillset(&sig_end.sa_mask);
    sig_end.sa_flags = 0;
    sigaction(SIGTERM, &sig_end, NULL);

    struct sigaction sif_suicide;
    sif_suicide.sa_handler = sig_suicide_handler;
    sigfillset(&sif_suicide.sa_mask);
    sif_suicide.sa_flags = 0;
    sigaction(SIGUSR1, &sif_suicide, NULL);

    key_t gate_key = grab_key_from_file(GATE_KEY_FILE_NAME);
    if (gate_key < 0) {
        print_error("Cant grab key");
        _exit(1);
    }
    int gate_semaphore_id = get_semaphore(gate_key);

    if (semop(shm_dron_info_semaphore_id, &SEM_LOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        exit(1);
    }

    //Todo Problem jeśli my_slot ma zły przedział trzeba osbłurzyć!
    DronData_Location location = p_shm_dron_info->drones[my_slot].dron_location;

    if (semop(shm_dron_info_semaphore_id, &SEM_UNLOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        exit(1);
    }

    print_msg("Started");
    DronData_Location next_location;
    while (1) {
        switch (location) {
            case LOCATION_BASE:
                sleep(5); //Charging
                print_msg("Charging done");
                next_location = LOCATION_MISSION;

                if (semop(gate_semaphore_id, &SEM_LOCK, 1) == -1) {
                    print_error("While waiting for semaphore: semop -1");
                    exit(1);
                }

                p_shm_dron_info->drones[my_slot].loading_cycles_left--;
                p_shm_dron_info->drones[my_slot].last_update = time(NULL);

                if (semop(gate_semaphore_id, &SEM_UNLOCK, 1) == -1) {
                    print_error("While leaving semaphore: semop +1");
                    exit(1);
                }

                break;
            case LOCATION_MISSION:
                sleep(5); //On mission
                print_msg("Mission done");
                next_location = LOCATION_BASE;

                if (semop(gate_semaphore_id, &SEM_LOCK, 1) == -1) {
                    print_error("While waiting for semaphore: semop -1");
                    exit(1);
                }

                p_shm_dron_info->missions_completed_count++;

                if (semop(gate_semaphore_id, &SEM_UNLOCK, 1) == -1) {
                    print_error("While leaving semaphore: semop +1");
                    exit(1);
                }

                break;

            default:
                //Todo Obsłurzyć
                _exit(1);
        }

        if (semop(gate_semaphore_id, &SEM_LOCK, 1) == -1) {
            print_error("While waiting for semaphore: semop -1");
            exit(1);
        }

        if (next_location == LOCATION_MISSION) {
            p_shm_dron_info->dron_in_base_count--;
        }
        if (next_location == LOCATION_BASE) {
            p_shm_dron_info->dron_in_base_count++;
        }
        p_shm_dron_info->drones[my_slot].dron_location = next_location;
        p_shm_dron_info->drones[my_slot].last_update = time(NULL);
        location = p_shm_dron_info->drones[my_slot].dron_location;

        sleep(2); //For testing

        if (semop(gate_semaphore_id, &SEM_UNLOCK, 1) == -1) {
            print_error("While leaving semaphore: semop +1");
            exit(1);
        }
    }

    return 0;
}
