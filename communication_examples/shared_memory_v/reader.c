#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 1234

int main() {
    // uzyskujemy dostęp do już istniejącej pamięci
    int shmid = shmget(SHM_KEY, 1024, 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // dołączamy segment
    char *shared = shmat(shmid, NULL, 0);
    if (shared == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    printf("Reader: odczytano: %s\n", shared);

    // odłączamy segment
    if (shmdt(shared) == -1) {
        perror("shmdt");
        exit(1);
    }

    // usuwamy segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
        exit(1);
    }

    printf("Reader: pamięć dzielona usunięta.\n");

    return 0;
}
