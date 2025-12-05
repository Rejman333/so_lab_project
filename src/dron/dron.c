#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/sem.h>

#include "printer.h"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

void suicide_handler(int sig) {
    print_msg(PROCESS_NAME, PROCESS_COLOR,"Suicided");
    exit(0);
}

void change_status() {
}

int main(int argc, char* argv[]) {
    pid_t group_pid = (pid_t)atoi(argv[1]);
    setpgid(0, group_pid);
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    struct sigaction sa;
    sa.sa_handler = suicide_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    key_t key = ftok("semfile", 1);
    if (key == -1) { perror("ftok"); exit(1); }

    int semid = semget(key, 1, IPC_CREAT |IPC_EXCL | 0666);
    if (semid >= 0) {

        print_msg("Semaphore created, with initial value of: 2.");
        if (semctl(semid, 0, SETVAL, 2) == -1) {
            perror("semctl SETVAL");
            exit(1);
        }
    } else {
        // Semafor już istnieje → otwórz go
        semid = semget(key, 1, 0666);
        if (semid == -1) { perror("semget"); exit(1); }
        print_msg("Semaphore already existed.");
    }

    struct sembuf lock = {0, -1, 0};   // wejdź
    struct sembuf unlock = {0, +1, 0}; // wyjdź

    print_msg("Started");

    while (1) {
        // próba wejścia
        print_msg("Waiting for semaphore...");
        if (semop(semid, &lock, 1) == -1) {
            perror("semop -1");
            exit(1);
        }

        // sekcja krytyczna
        print_msg(">>> Entered critical section");
        sleep(3);

        // wyjście
        if (semop(semid, &unlock, 1) == -1) {
            perror("semop +1");
            exit(1);
        }

        print_msg("<<< Left critical section");
        sleep(1);
    }

    return 0;
}