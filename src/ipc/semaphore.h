#pragma once

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int create_semaphore();

int get_semaphore();

void delete_semaphore(int semid);