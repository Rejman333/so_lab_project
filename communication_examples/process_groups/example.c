#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

void child_of_A(int group) {
    // Ustawiam PGID dziecka A na grupę
    setpgid(0, group);
    printf("[A-child] PID=%d, PGID=%d\n", getpid(), getpgid(0));
    sleep(3);
}

void child_of_B(int group) {
    // Ustaw PGID dziecka B
    setpgid(0, group);
    printf("[B-child] PID=%d, PGID=%d\n", getpid(), getpgid(0));
    sleep(3);
}

void process_A(int group) {
    // A dołącza do grupy
    setpgid(0, group);
    printf("[A] PID=%d, PGID=%d (leader = %d)\n", getpid(), getpgid(0), group);

    // Tworzy dwóch potomków
    pid_t p = fork();
    if (p == 0) child_of_A(group);

    p = fork();
    if (p == 0) child_of_A(group);

    sleep(3);
}

void process_B(int group) {
    // B też dołącza do grupy
    setpgid(0, group);
    printf("[B] PID=%d, PGID=%d\n", getpid(), getpgid(0));

    // B tworzy jedno dziecko
    pid_t p = fork();
    if (p == 0) child_of_B(group);

    sleep(3);
}

int main() {

    printf("[main] PID=%d, PGID=%d (main NIE jest w grupie)\n",
           getpid(), getpgid(0));

    pid_t A = fork();
    if (A == 0) {
        // Proces A: staje się liderem grupy
        setpgid(0, getpid());
        process_A(getpid());
        exit(0);
    }

    // Poczekaj, żeby A ustawił swoją grupę
    sleep(1);

    pid_t B = fork();
    if (B == 0) {
        process_B(A);  // B dołącza do grupy A (PGID = A)
        exit(0);
    }

    sleep(5);

    // <<< TU POPRAWKA >>>
    printf("[main] Killing group (PGID = %d)\n", A);
    kill(-A, SIGTERM);   // MINUS oznacza: sygnał dla grupy procesów

    return 0;
}
