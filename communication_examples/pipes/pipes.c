#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int pipefd[2];
    char buffer[100];

    // tworzymy pipe: pipefd[0] = read end, pipefd[1] = write end
    if (pipe(pipefd) == -1) {
        perror("pipe error");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork error");
        exit(1);
    }

    if (pid == 0) {
        // ---- PROCES DZIECKO (czytacz) ----
        close(pipefd[1]); // zamknij koniec do pisania

        // czytaj dane od rodzica
        ssize_t bytes = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0'; // zakończ string
            printf("Dziecko: odebrałem: %s\n", buffer);
        }

        close(pipefd[0]);
    } else {
        // ---- PROCES RODZIC (pisacz) ----
        close(pipefd[0]); // zamknij koniec do czytania

        const char *msg = "Witaj z procesu rodzica!";
        write(pipefd[1], msg, strlen(msg));

        close(pipefd[1]);
    }

    return 0;
}