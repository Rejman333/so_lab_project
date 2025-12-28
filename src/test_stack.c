#include "data_structures/stack.h"
#include "ipc/ipc.h"
#include "printer.h"

#include <stdlib.h>
#include <unistd.h>


int main() {
    setup_print("test", COLOR_GREEN);
    int semaphore_id = create_semaphore(1,0);
    int shm_id = shm_create(1,20);

    shm_destroy(shm_id);
    delete_semaphore(semaphore_id);
}
