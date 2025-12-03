#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/stat.h>  // S_IRUSR, S_IWUSR
#include <mqueue.h>

#define QUEUE_NAME "/myqueue"

int main() {
    mqd_t mq;
    struct mq_attr attr;
    char buffer[200];

    // konfiguracja kolejki
    attr.mq_flags = 0;        // blocking
    attr.mq_maxmsg = 10;      // max liczba wiadomości
    attr.mq_msgsize = 200;    // max rozmiar wiadomości
    attr.mq_curmsgs = 0;      // musi być 0

    // tworzymy kolejkę
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq == -1) {
        perror("mq_open");
        exit(1);
    }

    printf("Server: czekam na wiadomość w kolejce %s...\n", QUEUE_NAME);

    // odbieramy wiadomość
    ssize_t bytes = mq_receive(mq, buffer, sizeof(buffer), NULL);
    if (bytes >= 0) {
        buffer[bytes] = '\0';
        printf("Server: odebrano: %s\n", buffer);
    } else {
        perror("mq_receive");
    }

    // sprzątamy
    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    printf("Server: kolejka usunięta, koniec.\n");

    return 0;
}