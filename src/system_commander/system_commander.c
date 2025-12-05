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

    setup_print(PROCESS_NAME, PROCESS_COLOR);

    print_msg("Started");

    while (1) {
        print_msg("");
        sleep(1);
    }

    return 0;
}