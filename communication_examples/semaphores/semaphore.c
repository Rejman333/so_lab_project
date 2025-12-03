#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>    // O_CREAT
#include <semaphore.h>
#include <sys/stat.h> // S_IRUSR, S_IWUSR

#define SEM_NAME "/mysemaphore"

int main() {
    sem_t *sem;

    // tworzymy semafor o nazwie /mysemaphore, wartość początkowa = 0
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // ---- PROCES DZIECKO ----
        printf("Dziecko: czekam na semafor...\n");

        sem_t *sem_child = sem_open(SEM_NAME, 0);  // otwieramy istniejący semafor

        sem_wait(sem_child);   // czekamy na sygnał od rodzica

        printf("Dziecko: dostałem sygnał! Kontynuuję.\n");

        sem_close(sem_child);
        exit(0);
    }

    // ---- PROCES RODZIC ----
    printf("Rodzic: robię coś przez 2 sekundy...\n");
    sleep(2);

    printf("Rodzic: wysyłam sygnał do dziecka\n");
    sem_post(sem);   // sygnał do dziecka

    sleep(1);

    sem_close(sem);
    sem_unlink(SEM_NAME);   // usuwamy semafor z systemu

    printf("Rodzic: koniec\n");

    return 0;
}
