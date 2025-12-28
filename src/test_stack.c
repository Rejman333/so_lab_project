#include "printer.h"
#include <sys/shm.h>
#include <errno.h>

// Zakładam, że masz te wrappery:
int shm_create(key_t key, size_t size);
void shm_destroy(int shm_id);
void *shm_attach(int shm_id);

int main(void) {
    setup_print("test", COLOR_GREEN);

    // 1) Stwórz SHM
    int shm_id = shm_create(1, 20);
    if (shm_id == -1) {
        print_error("shm_create failed");
        return 1;
    }

    // 2) Usuń SHM
    shm_destroy(shm_id);

    // 3) Spróbuj się do niego podpiąć -> shmat powinno zwrócić (void*)-1
    void *p = shm_attach(shm_id);
    if (!p) {
        // Tu powinien zadziałać Twój print_error z errno
        print_error("shm_attach after destroy failed (expected)");
        return 0;
    }

    // Jeśli jakimś cudem się nie wywaliło, to wymuś błąd dereferencją
    *(volatile int*)p = 123; // prawdopodobnie SIGSEGV
    return 0;
}