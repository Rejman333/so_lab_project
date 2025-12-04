#include <stdio.h>
#include <unistd.h>

#include "printer.h"

#define PROCESS_NAME "Operator"
#define PROCESS_COLOR COLOR_CYAN

#define START_DRON_NUMBER 3

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
    print_msg(PROCESS_NAME, PROCESS_COLOR,"Started");

    for (int i = 0; i < START_DRON_NUMBER; ++i) {
        creat_dron(getpid());
    }

    while (1) {
        print_msg(PROCESS_NAME, PROCESS_COLOR,"");
        sleep(1);
    }

    return 0;
}