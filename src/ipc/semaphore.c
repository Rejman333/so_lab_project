#include "semaphore.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>

#include "printer.h"

//Todo dokończyć implemntacje semafora uniwersalnosć
//Todo obsługa błedów i wyjątków


int create_semaphore(const char *file_name) {
    // Ensure the ftok file exists
    int fd = open(file_name, O_CREAT | O_RDWR, 0500);
    if (fd == -1) {
        perror("open semfile");
        exit(1);
    }
    close(fd);

    // Generate key
    key_t key = ftok("semfile", 1);
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    // Try to CREATE semaphore; fail if it exists
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1) {
        // Semaphore already exists OR other error
        perror("semget (semaphore already exists?)");
        exit(1);
    }

    // If we get here → semafor został *utworzony po raz pierwszy*
    printf("Semafor nowo utworzony. Ustawiam wartość = 2.\n");

    if (semctl(semid, 0, SETVAL, 2) == -1) {
        perror("semctl SETVAL");
        exit(1);
    }

    return semid;
}

int get_semaphore() {
    // Ensure the ftok file exists
    int fd = open("semfile", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open semfile");
        exit(1);
    }
    close(fd);

    // Generate key
    key_t key = ftok("semfile", 1);
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    // Try to CREATE semaphore; fail if it exists
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1) {
        // Semaphore already exists OR other error
        perror("semget (semaphore already exists?)");
        exit(1);
    }

    // If we get here → semafor został *utworzony po raz pierwszy*
    printf("Semafor nowo utworzony. Ustawiam wartość = 2.\n");

    if (semctl(semid, 0, SETVAL, 2) == -1) {
        perror("semctl SETVAL");
        exit(1);
    }

    return semid;
}

void delete_semaphore(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
        exit(1);
    }
}
