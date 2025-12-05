#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "printer.h"

#define PROCESS_NAME "Operator"
#define PROCESS_COLOR COLOR_CYAN

int max_drones = 10;

#define START_DRON_NUMBER 3

void add_max_drones_handler(int sig) {
    max_drones = max_drones * 2;
    print_msg("New max drones: %d", max_drones);
}

void decrese_max_drones_handler(int sig) {
    max_drones = max_drones / 2;
    print_msg("New max drones: %d", max_drones);
}

int creat_dron(int group_pid) {
    int dron_pid = fork();
    if (dron_pid == 0) {
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", group_pid);
        execl("./dron", "./dron", pid_str,NULL);
        perror("exec operator");
        return 1;
    }
    return dron_pid;
}

int main(int argc, char* argv[]) {
    setpgid(0, getpid());

    setup_print(PROCESS_NAME, PROCESS_COLOR);

    struct sigaction sa1;
    sa1.sa_handler = add_max_drones_handler;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    sigaction(SIGUSR1, &sa1, NULL);

    struct sigaction sa2;
    sa2.sa_handler = decrese_max_drones_handler;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGUSR2, &sa2, NULL);

    print_msg("Started");

    for (int i = 0; i < START_DRON_NUMBER; ++i) {
        creat_dron(getpid());
    }

    while (1) {
        print_msg("");
        sleep(1);
    }

    return 0;
}