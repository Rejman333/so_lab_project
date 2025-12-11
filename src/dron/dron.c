#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/sem.h>

#include "printer.h"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

#define DRON_KEY_FILE "dron_semaphore_key"


#define MAXIMUM_LOADING_TIMES 5
#define BATTERY_INTERVAL 100;

typedef enum {
    LOCATION_UNDEFINE,
    LOCATION_BASE,
    LOCATION_MISSION
} Location;

int semaphore_id = -1;

Location location = LOCATION_UNDEFINE;

int max_loading_circles = 10;
int current_loading_circle = 0;

// int maximum_charge_time = 10000000;
// int boundary_charge_time = lroundf(maximum_charge_time * 0.2f);
// int current_charge_time = 0;
//
// int maximum_flight_time = lroundf(maximum_charge_time * 2.5f);
// int boundary_flight_time = lroundf(maximum_flight_time * 0.2f);
// int current_flight_time = 0;


int mission_time = 0;

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
    int semaphore_id = atoi(argv[1]);

    srand(time(NULL));
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
        time_t rawtime = time(NULL);

        // switch (location) {
        //     case LOCATION_BASE:
        //         print_msg("Charging Started");
        //         pthread_t charging_thread;
        //         if (pthread_create(&charging_thread, NULL, thread_charge_battery, NULL) != 0) {
        //             print_error("Thread error");
        //             return 1;
        //         }
        //         pthread_join(charging_thread, NULL);
        //         print_msg("Charging Finished");
        //         break;
        //     case LOCATION_MISSION:
        //         break;
        //     default:
        //         print_error("Undefined Location");
        //         _exit(1);
        // }

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
