#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/sem.h>

#include "ipc.h"
#include "printer.h"

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
    if (argc < 2) {
        print_error("No location provided\n");
        return 1;
    }

    Location location = (Location)atoi(argv[1]);

    srand(time(NULL));

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


    print_msg("Started");
    while (1) {
        print_msg("Waiting for semaphore...");
        if (semop(gate_semaphore_id, &SEM_LOCK, 1) == -1) {
            print_error("While waiting for semaphore: semop -1");
            exit(1);
        }

        print_msg(">>> Entered critical section");
        sleep(3);

        if (semop(gate_semaphore_id, &SEM_UNLOCK, 1) == -1) {
            print_error("While leaving semaphore: semop +1");
            exit(1);
        }

        print_msg("<<< Left critical section");
        sleep(1);
    }

    return 0;
}
