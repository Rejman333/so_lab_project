#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

#include "printer.h"

#define PROCESS_NAME "Main"
#define PROCESS_COLOR COLOR_BLUE

int creat_operator() {
    int operator_pid = fork();
    if (operator_pid == 0) {
        execl("./operator", "./operator", NULL);
        perror("exec operator");
        return 1;
    }

    return operator_pid;
}

int creat_system_commander(int group_pid) {
    int system_commander_pid = fork();
    if (system_commander_pid == 0) {
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", group_pid);
        execl("./system_commander", "./operator", pid_str,NULL);
        perror("exec operator");
        return 1;
    }
    return system_commander_pid;
}

void kill_all_in_group(int group_pid) {
    print_msg(PROCESS_NAME, PROCESS_COLOR, "Killing group (PGID = %d)", group_pid);
    kill(-group_pid, SIGTERM);
}

int main(int argc, char *argv[]) {
    print_msg(PROCESS_NAME,PROCESS_COLOR, "Started");

    int operator_pid = creat_operator();
    int group_pid = operator_pid;

    int system_commander_pid = creat_system_commander(group_pid);
    sleep(5);

    kill_all_in_group(group_pid);
    return 0;
}
