#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#define SHM_KEY 1234

int main() {
    // tworzymy segment pamięci dzielonej
    int shmid = shmget(SHM_KEY, 1024, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // dołączamy segment do przestrzeni adresowej
    char *shared = shmat(shmid, NULL, 0);
    if (shared == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    // zapisujemy dane
    strcpy(shared, "Hello from writer via shared memory!");

    printf("Writer: zapisano do pamięci: %s\n", shared);

    // odłączamy segment
    if (shmdt(shared) == -1) {
        perror("shmdt");
        exit(1);
    }

    return 0;
}
