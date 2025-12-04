#pragma once
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct DronMemory {
    int max_flight_time;
    int max_charge_cycles;
};

void create_dron(int max_flight_time, int max_charge_cycles) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        exit(1);
    }
    if (pid == 0) {
        //Childe
        printf("Child: running dron program...\n");
        execl("./dron", "dron", (char *) NULL);
        perror("exec failed");
    } else {
        printf("Parent: child created with PID %d\n", pid);
    }
}
