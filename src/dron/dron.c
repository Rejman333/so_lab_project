#include <stdlib.h>
#include <unistd.h>

#include "printer.h"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

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