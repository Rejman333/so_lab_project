#include "ipc.h"

#include <unistd.h>
#include <sys/shm.h>

#include "printer.h"

int shm_create(key_t key, size_t size) {
    int shmid = shmget(key, size, 0600 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1) {
        print_error("Failed to create sheared memory");
        _exit(1);
    }
    return shmid;
};

int shm_open_existing(key_t key) {
    int shmid = shmget(key, 0, 0600);
    if (shmid == -1) {
        print_error("shm_open_existing: shmget");
        return -1;
    }
    return shmid;
}

void *shm_attach(int shmid) {
    void *addr = shmat(shmid, NULL, 0);

    if (addr == (void *) -1) {
        print_error("shm_attach: shmat");
        return NULL;
    }

    return addr;
}

int shm_detach(const void *addr) {
    if (shmdt(addr) == -1) {
        print_error("shm_detach: shmdt");
        return -1;
    }
    return 0;
}

int shm_destroy(int shmid) {
    // Mark for deletion
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        print_error("shm_destroy: shmctl(IPC_RMID)");
        return -1;
    }
    return 0;
}
