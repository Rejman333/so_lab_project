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

    // ustaw wartość semafora na 0 przy pierwszym uruchomieniu
    semctl(semid, 0, SETVAL, 0);

    printf("V: przygotowuję się... (3 sek)\n");
    sleep(3);

    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = +1;  // V operation: increment, signals waiting process
    op.sem_flg = 0;

    printf("V: wysyłam sygnał!\n");
    if (semop(semid, &op, 1) == -1) {
        perror("semop +1");
        exit(1);
    }

    return 0;
}
