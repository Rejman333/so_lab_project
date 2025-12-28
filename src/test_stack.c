#include "data_structures/stack.h"
#include "ipc/ipc.h"
#include "printer.h"

#include <stdlib.h>
#include <unistd.h>


int main() {
    setup_print("test", COLOR_GREEN);
    int semaphore_id = semaphore_create(1,0);
    int shm_id = shm_create(1,20);

    shm_destroy(shm_id);
    semaphore_delete(semaphore_id);
}
