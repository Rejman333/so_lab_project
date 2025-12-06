#pragma once

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int create_semaphore(const char *file_name, int semaphore_starting_value);

int get_semaphore(const char *file_name);

void delete_semaphore(int semaphore_id);