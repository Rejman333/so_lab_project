#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "printer.h"

#define PROCESS_NAME "System Commander"
#define PROCESS_COLOR COLOR_YELLOW

void sig_end_handler(int sig) {
    print_msg("Received signal: %d, shutting down...", sig);
    exit(0);
}

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
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    struct sigaction sig_end;
    sig_end.sa_handler = sig_end_handler;
    sigemptyset(&sig_end.sa_mask);
    sig_end.sa_flags = 0;
    sigaction(SIGTERM, &sig_end, NULL);
    sigaction(SIGINT, &sig_end, NULL);

    print_msg("Started");
    while (1) {
        print_msg("");
        sleep(1);
    }

    return 0;
}