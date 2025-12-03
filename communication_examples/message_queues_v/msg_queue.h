#pragma once
#define QUEUE_KEY 1234   // dowolny stały numer klucza

struct msgbuf {
    long mtype;         // typ wiadomości (musi być > 0)
    char mtext[200];    // treść wiadomości
};
