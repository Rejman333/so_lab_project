#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

int main() {
    key_t key = ftok("semfile", 1);
    if (key == -1) { perror("ftok"); exit(1); }

    int semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) { perror("semget"); exit(1); }

    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;  // P operation: decrement, blocks if zero
    op.sem_flg = 0;

    printf("P: czekam na semafor...\n");
    if (semop(semid, &op, 1) == -1) {
        perror("semop -1");
        exit(1);
    }

    printf("P: dostałem sygnał — kontynuuję!\n");

    // usuń semafor (tylko jeśli to ostatni proces)
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
        exit(1);
    }

    printf("P: semafor usunięty.\n");

    return 0;
}
