#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    msg.mtype = 1;  // typ wiadomości
    strcpy(msg.mtext, "Pozdrowienia z procesu sender!");

    if (msgsnd(qid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
        perror("msgsnd");
        exit(1);
    }

    printf("Sender: wysłano wiadomość: %s\n", msg.mtext);

    return 0;
}
