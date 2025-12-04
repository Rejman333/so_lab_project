#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void send_add_drones(const pid_t dron_pid) {
    if (kill(dron_pid,SIGUSR1) < 0) {
        perror("kill error");
        exit(1);
    }
}

void send_subtract_drones(const pid_t dron_pid) {
    if (kill(dron_pid,SIGUSR2) < 0) {
        perror("kill error");
        exit(1);
    }
}

void send_suicide(const pid_t dron_pid) {
    if (kill(dron_pid,SIGUSR1) < 0) {
        perror("kill error");
        exit(1);
    }
}

int main(int argc, char* argv[]) {

    setpgid(0, getpid());

    while (1) {
        printf("[system_commander] PID=%d, PGID=%d\n", getpid(), getpgid(0));
        sleep(1);
    }

    return 0;
}