#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "printer.h"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

void suicide_handler(int sig) {
    print_msg(PROCESS_NAME, PROCESS_COLOR,"Suicided");
    exit(0);
}

int main(int argc, char* argv[]) {
    pid_t group_pid = (pid_t)atoi(argv[1]);
    setpgid(0, group_pid);

    struct sigaction sa;
    sa.sa_handler = suicide_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGUSR1, &sa, NULL);

    print_msg(PROCESS_NAME, PROCESS_COLOR,"Started");

    while (1) {
        print_msg(PROCESS_NAME, PROCESS_COLOR,"");
        sleep(1);
    }

    return 0;
}