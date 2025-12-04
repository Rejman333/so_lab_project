#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("[dron] PID=%d, PGID=%d\n", getpid(), getpgid(0));
    return 0;
}
