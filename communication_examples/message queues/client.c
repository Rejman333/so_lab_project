#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#define QUEUE_NAME "/myqueue"

int main() {
    mqd_t mq;

    // otwieramy istniejącą kolejkę
    mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == -1) {
        perror("mq_open");
        exit(1);
    }

    const char *msg = "Pozdrowienia z klienta POSIX MQ!";
    if (mq_send(mq, msg, strlen(msg), 0) == -1) {
        perror("mq_send");
    } else {
        printf("Client: wysłano wiadomość.\n");
    }

    mq_close(mq);
    return 0;
}
