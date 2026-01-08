#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define STACK_KEY_FILE_NAME "stack_key"

#define PROCESS_NAME "System Commander"
#define PROCESS_COLOR COLOR_ORANGE

#define LOG_FILE_NAME "log.txt"

#define EINTR 4


static volatile sig_atomic_t got_shutdown_requested = 0;

int operator_pid = -1;

SHM_AllDronesData *shm_all_drones_data = NULL;
int shm_all_drones_data_id = -1;
Stack *shm_stack = NULL;
int shm_stack_id = -1;
int shm_all_drones_data_semaphore_id = -1;

void shutdown_request_handler(int sig) {
    got_shutdown_requested = 1;
}

int parse_positive_int(const char *arg, const char *name) {
    char *end;

    errno = 0;
    const long value = strtol(arg, &end, 10);

    if (errno != 0 || *end != '\0' || value <= 0 || value > INT_MAX) {
        print_error("Invalid value for %s: %s\n", name, arg);
        exit(EXIT_FAILURE);
    }

    return (int) value;
}

void process_argv_operator_pid(int argc, char *argv[]) {
    if (argc < 2) {
        print_error("Operator pid not specified");
        exit(EXIT_FAILURE);
    }

    const int pid = parse_positive_int(argv[1], "operator_pid");

    if (pid <= 0) {
        print_error("Operator pid must be > 0");
        exit(EXIT_FAILURE);
    }

    operator_pid = pid;
}

int get_shm_all_drones_data() {
    const key_t shm_all_drones_data_key = grab_key_from_file(ALL_DRONES_DATA_FILE_NAME);
    if (shm_all_drones_data_key < 0) {
        print_error("Cant grab key");
        return -1;
    }


    shm_all_drones_data_id = shm_get(shm_all_drones_data_key);
    if (shm_all_drones_data_id < 0) {
        print_error("Failed to get shm for all_drones_data");
        return -1;
    }

    shm_all_drones_data = shm_attach(shm_all_drones_data_id);
    if (shm_all_drones_data == NULL) {
        print_error("Failed to attach shm for all_drones_data");
        return -1;
    }

    const key_t shm_stack_key = grab_key_from_file(STACK_KEY_FILE_NAME);
    if (shm_stack_key < 0) {
        print_error("Cant grab key");
    }
    shm_stack_id = shm_get(shm_stack_key);
    if (shm_stack_id < 0) {
        print_error("Failed to get shm for stack");
        return -1;
    }
    shm_stack = shm_attach(shm_stack_id);
    if (shm_stack == NULL) {
        print_error("Failed to attach shm for stack");
        return -1;
    }

    shm_all_drones_data_semaphore_id = semaphore_get(shm_all_drones_data_key);
    if (shm_all_drones_data_semaphore_id < 0) {
        print_error("Cant get semaphore for all_drones_data");
        return -1;
    }
    return 0;
}

int send_add_drones(const pid_t pid) {
    print_msg("Sending add_drones to operator on pid: %d", pid);
    return (kill(pid, SIGUSR1) < 0) ? -1 : 0;
}

int send_subtract_drones(const pid_t pid) {
    print_msg("Sending subtract_drones to operator on pid: %d", pid);
    return (kill(pid, SIGUSR2) < 0) ? -1 : 0;
}

int send_suicide(const pid_t dron_pid) {
    print_msg("Sending suicide to dron on pid: %d", dron_pid);
    return (kill(dron_pid, SIGUSR1) < 0) ? -1 : 0;
}

int mission_wait_seconds_interruptible(const int sec) {
    struct timespec req = {.tv_sec = sec, .tv_nsec = 0};
    struct timespec rem = {0};

    while (!got_shutdown_requested) {
        if (nanosleep(&req, &rem) == 0) return 0;
        if (errno == EINTR) {
            req = rem;
            continue;
        }
        return -1;
    }
    return 1;
}

int pick_dron_pid_from_shm() {
    if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("Mission: semaphore_lock failed");
        return -1;
    }

    const int dron_pid = SHM_AllDronesData_get_dron_pid(shm_all_drones_data);

    if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("Mission: semaphore_unlock failed");
        return -1;
    }

    return dron_pid;
}

int mission_step_increase_drones(const pid_t op_pid, const int repeats, const int delay_sec) {
    print_msg("Step: Increasing drones (%d times, every %ds)", repeats, delay_sec);

    for (int i = 0; i < repeats; ++i) {
        if (got_shutdown_requested) return 0;

        int w = mission_wait_seconds_interruptible(delay_sec);
        if (w == 1) return 0;
        if (w == -1) return -1;

        if (send_add_drones(op_pid) == -1) {
            print_error("Mission: send_add_drones failed");
        }
    }
    return 0;
}

int mission_step_kill_drones(const int repeats, const int delay_sec) {
    print_msg("Step: Removing drones (suicide) (%d times, every %ds)", repeats, delay_sec);

    for (int i = 0; i < repeats; ++i) {
        if (got_shutdown_requested) return 0;

        int w = mission_wait_seconds_interruptible(delay_sec);
        if (w == 1) return 0;
        if (w == -1) return -1;

        int dron_pid = pick_dron_pid_from_shm();
        if (dron_pid == -1) {
            return -1;
        }

        if (dron_pid < 0) {
            print_msg("Mission: no drone PID available in SHM");
            continue;
        }

        if (send_suicide((pid_t) dron_pid) == -1) {
            print_error("Mission: send_suicide failed for pid=%d", dron_pid);
        }
    }
    return 0;
}

int mission_step_decrease_drones(const pid_t op_pid, const int repeats, const int delay_sec) {
    print_msg("Step: Decreasing drones (%d times, every %ds)", repeats, delay_sec);

    for (int i = 0; i < repeats; ++i) {
        if (got_shutdown_requested) return 0;

        int w = mission_wait_seconds_interruptible(delay_sec);
        if (w == 1) return 0;
        if (w == -1) return -1;

        if (send_subtract_drones(op_pid) == -1) {
            print_error("Mission: send_subtract_drones failed");
        }
    }
    return 0;
}

int mission_step_decrease_drones_to_0(const pid_t op_pid, const int delay_sec) {
    print_msg("Step: Decreasing drones (every %ds)", delay_sec);

    if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("Mission: semaphore_lock failed");
        return -1;
    }
    int maximum_dron_in_base_count = shm_all_drones_data->maximum_dron_in_base_count;
    if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("Mission: semaphore_unlock failed");
        return -1;
    }

    while (maximum_dron_in_base_count > 0) {
        if (got_shutdown_requested) return 0;

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("Mission: semaphore_lock failed");
            return -1;
        }

        maximum_dron_in_base_count = shm_all_drones_data->maximum_dron_in_base_count;

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("Mission: semaphore_unlock failed");
            return -1;
        }

        if (send_subtract_drones(op_pid) == -1) {
            print_error("Mission: send_subtract_drones failed");
        }

        int w = mission_wait_seconds_interruptible(delay_sec);
        if (w == 1) return 0;
        if (w == -1) return -1;
    }

    return 0;
}

int run_mission_plan(const pid_t op_pid) {
    print_msg("=== Mission Plan: START ===");

    if (mission_step_increase_drones(op_pid, 3, 5) == -1) return -1;
    if (mission_step_kill_drones(10, 5) == -1) return -1;
    if (mission_step_decrease_drones_to_0(op_pid, 5) == -1) return -1;

    print_msg("=== Mission Plan: FINISHED ===");
    return 0;
}

void cleanup_ipc_attachments() {
    if (shm_all_drones_data) {
        if (shm_detach(shm_all_drones_data) != 0) {
            print_error("cleanup: shm_detach(all_drones_data) failed");
        }
        shm_all_drones_data = NULL;
    }

    if (shm_stack) {
        if (shm_detach(shm_stack) != 0) {
            print_error("cleanup: shm_detach(stack) failed");
        }
        shm_stack = NULL;
    }

    shm_all_drones_data_id = -1;
    shm_stack_id = -1;
    shm_all_drones_data_semaphore_id = -1;
}

int close_main(const int exit_code) {
    cleanup_ipc_attachments();

    logger_shutdown();
    exit(exit_code);
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);

    if (logger_initialize(LOG_FILE_NAME) != 0) {
        print_error("Logger initialization failed");
        close_main(EXIT_FAILURE);
    }

    process_argv_operator_pid(argc, argv);

    struct sigaction sa = {0};
    sa.sa_handler = shutdown_request_handler;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        print_error("sigaction(SIGINT) failed");
        close_main(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        print_error("sigaction(SIGTERM) failed");
        close_main(EXIT_FAILURE);
    }

    if (get_shm_all_drones_data() != 0) {
        print_error("Failed to get/attach shm/semaphore");
        close_main(EXIT_FAILURE);
    }

    const int mission_result = run_mission_plan(operator_pid);


    if (got_shutdown_requested) {
        print_msg("Shutdown requested -> closing gracefully");
        close_main(EXIT_SUCCESS);
    }

    if (mission_result == -1) {
        print_error("Mission plan failed");
        close_main(EXIT_FAILURE);
    }

    close_main(EXIT_SUCCESS);
    return 0;
}
