#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>

#include "ipc.h"
#include "printer.h"

#define PROCESS_NAME "Operator"
#define PROCESS_COLOR COLOR_CYAN

#define GATE_FILE "gate"
#define GATE_NUMBER 2

#define START_DRON_NUMBER 3

int max_drones = 10;
int gate_semaphore_id = -1;

void sig_end_handler(int sig) {
    print_msg("Received signal: %d, shutting down...", sig);

    if (gate_semaphore_id > 0) {
        print_msg("Removing semaphore %d", gate_semaphore_id);
        delete_semaphore(gate_semaphore_id);
        gate_semaphore_id = -1;
    }
    exit(0);
}

void add_max_drones_handler(int sig) {
    max_drones = max_drones * 2;
    print_msg("New max drones: %d", max_drones);
}

void decrease_max_drones_handler(int sig) {
    max_drones = max_drones / 2;
    print_msg("New max drones: %d", max_drones);
}

int creat_dron(int gate_semaphore_id) {
    int dron_pid = fork();
    if (dron_pid == 0) {
        char sem_str[32];
        snprintf(sem_str, sizeof(sem_str), "%d", gate_semaphore_id);

        execl("./dron", "./dron", sem_str, NULL);
        perror("exec dron");
        exit(1);
    }
    return dron_pid;
}

void* thread_func(void* arg) {
    print_msg("Dron maker started");
    int number_of_drones_in_base = 0;
    while (1) {
        if (number_of_drones_in_base < max_drones) {
            creat_dron(gate_semaphore_id);
            number_of_drones_in_base++;
        }
        sleep(1);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    struct sigaction sig_end;
    sig_end.sa_handler = sig_end_handler;
    sigfillset(&sig_end.sa_mask);
    sig_end.sa_flags = 0;
    sigaction(SIGINT, &sig_end, NULL);
    sigaction(SIGTERM, &sig_end, NULL);

    struct sigaction sig_add_max_drones;
    sig_add_max_drones.sa_handler = add_max_drones_handler;
    sigemptyset(&sig_add_max_drones.sa_mask);
    sig_add_max_drones.sa_flags = 0;
    sigaction(SIGUSR1, &sig_add_max_drones, NULL);

    struct sigaction sig_decrease_max_drones_handler;
    sig_decrease_max_drones_handler.sa_handler = decrease_max_drones_handler;
    sigemptyset(&sig_decrease_max_drones_handler.sa_mask);
    sig_decrease_max_drones_handler.sa_flags = 0;
    sigaction(SIGUSR2, &sig_decrease_max_drones_handler, NULL);

    gate_semaphore_id = create_semaphore(GATE_FILE, GATE_NUMBER);

    pthread_t tid;

    if (pthread_create(&tid, NULL, thread_func, NULL) != 0) {
        print_error("Thread error");
        return 1;
    }
    pthread_detach(tid);

    print_msg("Started");

    for (int i = 0; i < START_DRON_NUMBER; ++i) {
        creat_dron(gate_semaphore_id);
    }

    while (1) {
        print_msg("");
        sleep(1);
    }

    delete_semaphore(gate_semaphore_id);
    return 0;
}