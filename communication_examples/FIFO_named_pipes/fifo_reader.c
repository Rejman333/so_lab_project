#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define FIFO_PATH "/tmp/myfifo"

int main() {
    char buffer[200];

    int fd = open(FIFO_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';  // zakończ string
        printf("Reader: odebrałem: %s\n", buffer);
    }

    close(fd);
    return 0;
}