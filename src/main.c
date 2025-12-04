#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>


int main(int argc, char* argv[]) {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("Current working dir: %s\n", cwd);
    printf("[main] PID=%d, PGID=%d (main NIE jest w grupie)\n",
           getpid(), getpgid(0));


    pid_t system_commander_pid = fork();
    if (system_commander_pid == 0) {
        execl("./system_commander","./operator",NULL);
        perror("exec operator");
        return 1;
    }

    pid_t operator_pid = fork();
    if (operator_pid == 0) {
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", system_commander_pid);
        execl("./operator","./operator",pid_str);

        perror("exec operator");
        return 1;
    }

    sleep(5);

    printf("[main] Killing group (PGID = %d)\n", system_commander_pid);
    kill(-system_commander_pid, SIGTERM);
    return 0;
}
