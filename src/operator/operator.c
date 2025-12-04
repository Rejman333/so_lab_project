#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

    printf("Starting Operator\n");
    pid_t system_commander_pid = (pid_t)atoi(argv[1]);
    setpgid(0, system_commander_pid);
    while (1) {
        printf("[operator] PID=%d, PGID=%d\n", getpid(), getpgid(0));
        sleep(1);
    }

    return 0;
}