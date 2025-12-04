#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void send_add_drones() {
    if (kill(dron_pid,SIGUSR1) < 0) {
        perror("kill error");
        exit(1);
    }
}

void send_subtract_drones() {
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

int main(void) {
    printf("Hello, World!\n");
    return 0;
}