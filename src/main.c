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

void cleanup() {
    print_msg("Cleanup started...");

    pid_t pid;
    while ((pid = wait(NULL)) > 0) {
        print_msg("Process ended: %d", pid);
    }

    print_msg("Cleanup complete.");
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

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    signal(SIGINT, SIG_IGN);
    //Optional
    //signal(SIGTERM, SIG_IGN);
    print_msg("Started");

    operator_pid = creat_operator();
    system_commander_pid = creat_system_commander();

    cleanup();
    return 0;
}
