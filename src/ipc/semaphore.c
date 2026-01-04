#include <errno.h>
#include "ipc.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/sem.h>

#include "printer.h"

#define EINTR 4 //Clion is blind
#define EEXIST 17 //Clion is blind

int semaphore_delete(const int semaphore_id) {
    if (semctl(semaphore_id, 0, IPC_RMID) == -1) {
        print_error("Failed to delete semaphore ID %d", semaphore_id);
        return -1;
    }
    return 0;
}

int semaphore_create(const key_t key, const int semaphore_starting_value) {
    const int semaphore_id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
    if (semaphore_id == -1) {
        if (errno == EEXIST) {
            print_error("Failed to create semaphore on key=%d: already exists", (int)key);
        } else {
            print_error("Failed to create semaphore on key=%d", (int)key);
        }
    }

    union semun arg;
    arg.val = semaphore_starting_value;

    if (semctl(semaphore_id, 0, SETVAL, arg) == -1) {
        print_error("Semaphore ID=%d: SETVAL failed", semaphore_id);
        if (semaphore_delete(semaphore_id) == -1) {
            print_error("Semaphore ID=%d: IPC_RMID cleanup after SETVAL failure also failed", semaphore_id);
        }
        return -1;
    }
    print_msg("Semaphore ID: %d created, with starting value of: %d", semaphore_id, semaphore_starting_value);

    return semaphore_id;
}

int semaphore_get(const key_t key) {
    // Try to CREATE semaphore; fail if it exists
    const int semaphore_id = semget(key, 1, 0);
    if (semaphore_id == -1) {
        // Semaphore already exists OR other error
        print_error("Failed to get a semaphore on key: %d", semaphore_id);
        return -1;
    }

    return semaphore_id;
}



int semaphore_lock(const int semaphore_id) {
    for (;;) {
        if (semop(semaphore_id, &SEM_LOCK, 1) == 0) return 0;
        if (errno == EINTR) {
            continue;
        };
        return -1;
    }
}

int semaphore_unlock(const int semaphore_id) {
    for (;;) {
        if (semop(semaphore_id, &SEM_UNLOCK, 1) == 0) return 0;
        if (errno == EINTR) {
            print_error("Semaphore_unlock has a problem");
            continue;
        };
        return -1;
    }
}