#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "printer.h"

#define PROCESS_NAME "Main"
#define PROCESS_COLOR COLOR_BLUE
#define DRON_SEMAPHORE_MAX_VALUE 2

int operator_pid = -1;
int system_commander_pid = -1;

int starting_drones_count = 10;
int resupply_interval = 1000000; // 1s in us

int maximum_charge_time = 10000000; //10s in us
int max_loading_circles = 5;

void process_argv(int argc, char* argv[]) {
    if (argc > 1) {
        starting_drones_count = atoi(argv[1]);
        if (starting_drones_count <= 0) {
            print_error("starting_drones_count must be > 0");
            exit(1);
        }
    }

    if (argc > 2) {
        resupply_interval = atoi(argv[2]);
        if (resupply_interval <= 0) {
            print_error("resupply_interval must be > 0");
            exit(1);
        }
    }

    if (argc > 3) {
        maximum_charge_time = atoi(argv[3]);
        if (maximum_charge_time <= 0) {
            print_error("maximum_charge_time must be > 0");
            exit(1);
        }
    }

    if (argc > 4) {
        max_loading_circles = atoi(argv[4]);
        if (max_loading_circles <= 0) {
            print_error("max_loading_circles must be > 0");
            exit(1);
        }
    }

    if (argc > 5) {
        print_error("Maximum argument count is 4");
        exit(1);
    }

    print_msg("=== Starting Configuration ===");
    print_msg("starting_drones_count = %d", starting_drones_count);
    print_msg("resupply_interval = %d", resupply_interval);
    print_msg("maximum_charge_time = %d", maximum_charge_time);
    print_msg("max_loading_circles = %d", max_loading_circles);

}

int creat_operator() {
    int pid = fork();
    if (pid == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);

        execl("./operator", "./operator", NULL);
        perror("exec operator");
        exit(1);
    }
    return pid;
}

int creat_system_commander() {
    int pid = fork();
    if (pid == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);

        execl("./system_commander", "./system_commander", NULL);
        perror("exec system_commander");
        exit(1);
    }
    return pid;
}

void cleanup() {
    print_msg("Cleanup started...");

    pid_t pid;
    while ((pid = wait(NULL)) > 0) {
        print_msg("Process ended: %d", pid);
    }

    print_msg("Cleanup complete.");
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    process_argv(argc, argv);
    signal(SIGINT, SIG_IGN);
    //Optional
    //signal(SIGTERM, SIG_IGN);

    print_msg("Started");

    operator_pid = creat_operator();
    system_commander_pid = creat_system_commander();

    cleanup();

    return 0;
}
