#include "semaphore.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <stdbool.h>

#include "printer.h"

#define CREAT_FILE 1
#define DONT_CREAT_FILE 0

//Todo rethink this approach

static int grab_key_from_file(const char *file_name, bool create_if_not_exists) {

    int fd;

    if (create_if_not_exists) {
        fd = open(file_name, O_CREAT | O_RDWR, 0600);
    } else {
        fd = open(file_name, O_RDWR);
    }

    if (fd == -1) {
        if (create_if_not_exists)
            print_error("Failed to open or create key file: %s", file_name);
        else
            print_error("Key file does not exist: %s", file_name);
        exit(1);
    }

    close(fd);

    // Generate key
    key_t key = ftok(file_name, 1);
    if (key == -1) {
        print_error("Failed to generate key using ftok() for file: %s", file_name);
        exit(1);
    }

    return key;
}

int create_semaphore(const char *file_name, int semaphore_starting_value) {
    int key = grab_key_from_file(file_name, CREAT_FILE);

    // Try to CREATE semaphore; fail if it exists
    int semaphore_id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semaphore_id == -1) {
        // Semaphore already exists OR other error
        print_error("Failed to create a semaphore on key: %d, or it already existed", key);
        exit(1);
    }

    union semun arg;
    arg.val = semaphore_starting_value;

    if (semctl(semaphore_id, 0, SETVAL, arg) == -1) {
        print_error("Semaphore ID: %d, experienced an error", semaphore_id);
        exit(1);
    }
    print_msg("Semaphore ID: %d : 0 created, with starting value of: %d", semaphore_id, semaphore_starting_value);

    return semaphore_id;
}

int get_semaphore(const char *file_name) {
    int key = grab_key_from_file(file_name, DONT_CREAT_FILE);

    // Try to CREATE semaphore; fail if it exists
    int semaphore_id = semget(key, 1,0600);
    if (semaphore_id == -1) {
        // Semaphore already exists OR other error
        print_error("Failed to get a semaphore on key: %d", semaphore_id);
        exit(1);
    }

    return semaphore_id;
}

void delete_semaphore(int semaphore_id) {
    if (semctl(semaphore_id, 0, IPC_RMID) == -1) {
        print_error("Failed to delete semaphore ID %d", semaphore_id);
        exit(1);
    }
}
