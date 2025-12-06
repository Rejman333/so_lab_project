#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/sem.h>

#include "printer.h"
#include "semaphore.h"
#include "../ipc/semaphore.h"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

#define DRON_KEY_FILE "dron_semaphore_key"

void sig_end_handler(int sig) {
    print_msg("Received SIGTERM, shutting down...");
    _exit(0);
}

void sig_suicide_handler(int sig) {
    print_msg("Suicided");
    _exit(0);
}

int main(int argc, char* argv[]) {
    int semaphore_id = atoi(argv[1]);

    setup_print(PROCESS_NAME, PROCESS_COLOR);

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

    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, +1, 0};

    print_msg("Started");

    while (1) {
        print_msg("Waiting for semaphore...");
        if (semop(semaphore_id, &lock, 1) == -1) {
            print_error("While waiting for semaphore: semop -1");
            exit(1);
        }

        print_msg(">>> Entered critical section");
        sleep(3);

        if (semop(semaphore_id, &unlock, 1) == -1) {
            print_error("While leaving semaphore: semop +1");
            exit(1);
        }

        print_msg("<<< Left critical section");
        sleep(1);
    }

    return 0;
}