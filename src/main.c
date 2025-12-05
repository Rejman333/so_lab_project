#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/wait.h>

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
    print_msg("Killing group (PGID = %d)", group_pid);
    kill(-group_pid, SIGTERM);
}

int create_semaphore() {

    int fd = open("semfile", O_CREAT | O_RDWR, 0666);
    if (fd == -1) { perror("open semfile"); exit(1); }
    close(fd);

    // Generate key
    key_t key = ftok("semfile", 1);
    if (key == -1) { perror("ftok"); exit(1); }

    // Try to CREATE semaphore; fail if it exists
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1) {
        // Semaphore already exists OR other error
        perror("semget (semaphore already exists?)");
        exit(1);
    }

    print_msg("Semaphore created, with initial value of: 2.");

    if (semctl(semid, 0, SETVAL, 2) == -1) {
        perror("semctl SETVAL");
        exit(1);
    }

    return semid;
}

void delete_semaphore(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    int semid = create_semaphore();
    print_msg("Started");

    int operator_pid = creat_operator();
    int group_pid = operator_pid;

    int system_commander_pid = creat_system_commander(group_pid);
    sleep(25);

    kill_all_in_group(group_pid);

    pid_t pid;
    while ((pid = wait(NULL)) > 0) {
        print_msg("Process ended: %d", pid);
    }
    delete_semaphore(semid);
    return 0;
}
