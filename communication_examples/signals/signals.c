#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// Handler dla sygnału w procesie dziecka
void handler(int sig) {
    printf("Dziecko: Otrzymano sygnał %d (SIGUSR1)\n", sig);
}

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork error");
        exit(1);
    }

    if (pid == 0) {
        // ---- PROCES DZIECKO ----
        struct sigaction sa;
        sa.sa_handler = handler;    //Tutaj ustawia się handler
        sigemptyset(&sa.sa_mask);   //Nie blokujemy innych sygnałów w trakcie wykonywania tego
        sa.sa_flags = 0;            //Żadnych specjalnych zachowań

        // ustawiamy handler dla SIGUSR1
        sigaction(SIGUSR1, &sa, NULL);

        printf("Dziecko: czekam na sygnał...\n");

        // zawieszamy dziecko, żeby czekało na sygnał
        while (1) {
            pause(); // czekaj na sygnał
        }
    } 
    else {
        // ---- PROCES RODZIC ----
        printf("Rodzic: PID dziecka to %d\n", pid);

        sleep(1); // daj dziecku czas aby ustawiło handler

        printf("Rodzic: wysyłam SIGUSR1 do dziecka\n");
        kill(pid, SIGUSR1);

        sleep(1);

        printf("Rodzic: kończę\n");
    }

    return 0;
}