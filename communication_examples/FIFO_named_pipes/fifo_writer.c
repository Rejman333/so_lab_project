#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIFO_PATH "/tmp/myfifo"

int main() {
    // tworzymy FIFO jeśli nie istnieje
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo");
        // ignorujemy błąd jeśli fifo już istnieje
    }

    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    const char *msg = "Pozdrowienia z programu writer!";
    write(fd, msg, strlen(msg));

    close(fd);
    unlink(FIFO_PATH);
    return 0;
}