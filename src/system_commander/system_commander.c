#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "printer.h"

#define PROCESS_NAME "System Commander"
#define PROCESS_COLOR COLOR_YELLOW

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
    pid_t group_pid = (pid_t)atoi(argv[1]);
    setpgid(0, group_pid);

    print_msg(PROCESS_NAME, PROCESS_COLOR,"Started");

    while (1) {
        print_msg(PROCESS_NAME, PROCESS_COLOR,"");
        sleep(1);
    }

    return 0;
}