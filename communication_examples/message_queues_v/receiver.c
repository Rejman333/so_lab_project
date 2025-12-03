#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "msg_queue.h"

int main() {
    int qid = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if (qid == -1) {
        perror("msgget");
        exit(1);
    }

    struct msgbuf msg;

    // odbierz wiadomość typu 1
    if (msgrcv(qid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }

    printf("Receiver: odebrano: %s\n", msg.mtext);

    // usuwamy kolejkę z systemu wywołując IPC_RMID
    if (msgctl(qid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }

    printf("Receiver: kolejka usunięta.\n");

    return 0;
}
