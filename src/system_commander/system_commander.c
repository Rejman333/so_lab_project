#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define DRON_INFO_KEY_FILE_NAME "dron_info_key"

#define PROCESS_NAME "System Commander"
#define PROCESS_COLOR COLOR_YELLOW

void sig_end_handler(int sig) {
    print_msg("Received signal: %d, shutting down...", sig);
    exit(0);
}

void send_add_drones(const pid_t dron_pid) {
    if (kill(dron_pid,SIGUSR1) < 0) {
        perror("kill error");
        exit(1);
    }
}

void send_subtract_drones(const pid_t dron_pid) {
    if (kill(dron_pid,SIGUSR2) < 0) {
        perror("kill error");
        exit(1);
    }
}

void send_suicide(const pid_t dron_pid) {
    if (kill(dron_pid,SIGUSR1) < 0) {
        perror("kill error");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    struct sigaction sig_end;
    sig_end.sa_handler = sig_end_handler;
    sigemptyset(&sig_end.sa_mask);
    sig_end.sa_flags = 0;
    sigaction(SIGTERM, &sig_end, NULL);
    sigaction(SIGINT, &sig_end, NULL);

    key_t shm_dron_info_key = grab_key_from_file(DRON_INFO_KEY_FILE_NAME);
    if (shm_dron_info_key < 0) {
        print_error("Cant grab key");
    }

    int shm_dron_info_id = shm_open_existing(shm_dron_info_key);
    if (shm_dron_info_id < 0) {
        print_error("Cant open shm");
        _exit(1);
    }

    SHM_DronInfo *p_shm_dron_info = shm_attach(shm_dron_info_id);
    int shm_dron_info_semaphore_id = get_semaphore(shm_dron_info_key);

    print_msg("Started");

    while (1) {

        print_msg("Waiting for semaphore...");
        if (semop(shm_dron_info_semaphore_id, &SEM_LOCK, 1) == -1) {
            print_error("While waiting for semaphore: semop -1");
            exit(1);
        }

        print_msg(">>> Entered critical section");
        print_msg("Dron pid: %d",p_shm_dron_info->dron_state_array[0].pid);

        if (semop(shm_dron_info_semaphore_id, &SEM_UNLOCK, 1) == -1) {
            print_error("While leaving semaphore: semop +1");
            exit(1);
        }

        print_msg("<<< Left critical section");
        sleep(3);
    }

    return 0;
}