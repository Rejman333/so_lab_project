#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "ipc.h"
#include "printer.h"

#define CONFIG_KEY_FILE_NAME "config_key"
#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define STACK_KEY_FILE_NAME "stack_key"
#define GATE_KEY_FILE_NAME "gate_key"
#define GATE_FIFO_FILE_NAME "/tmp/gate_fifo"


#define PROCESS_NAME "Main"
#define PROCESS_COLOR COLOR_BLUE

#define GATE_SEMAPHORE_STARTING_VALUE 2
#define GATE_TIME_TO_PASS 20000

#define MAXIMUM_DRONES_IN_MEMORY 8
#define STARTING_DRONE_COUNT_DEFAULT 6
#define RESUPPLY_INTERVAL 1000000

#define DRON_WORK_INTERVAL 500000
#define MAXIMUM_CHARGE_TIME_DEFAULT 4000000
#define MAXIMUM_LOADING_CYCLES 3

#define LOG_FILE_NAME "log.txt"

SHM_Configuration *shm_configuration = NULL;
int shm_configuration_id = -1;
int shm_configuration_semaphore_id = -1;

SHM_AllDronesData *shm_all_drones_data = NULL;
int shm_all_drones_data_id = -1;
Stack *shm_stack = NULL;
int shm_stack_id = -1;
int shm_all_drones_data_semaphore_id = -1;

int gate_semaphore_id = -1;

int operator_pid = -1;
int system_commander_pid = -1;

FIFO_SEM fifo_sem = {
    .capacity = -1,
    .file_descriptor = -1,
};

int maximum_drones_in_memory = MAXIMUM_DRONES_IN_MEMORY;

static int parse_positive_int(const char *arg, const char *name) {
    char *end;

    errno = 0;
    const long value = strtol(arg, &end, 10);

    if (errno != 0 || *end != '\0' || value <= 0 || value > INT_MAX) {
        print_error("Invalid value for %s: %s\n", name, arg);
        exit(EXIT_FAILURE);
    }

    return (int) value;
}

static void print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "Options:\n"
            "  -n <count>   starting drones count\n"
            "  -r <time>    resupply interval\n"
            "  -c <time>    maximum charge time\n"
            "  -l <count>   max loading cycles\n"
            "  -g <time>    gate time to pass\n"
            "  -w <time>    drone work interval\n"
            "  -m <count>   maximum drones in memory\n"
            "  -h           show this help message\n",
            program_name
    );
}

void process_argv(SHM_Configuration *cfg, int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "n:r:c:l:g:w:m:h")) != -1) {
        switch (opt) {
            case 'n':
                cfg->starting_drones_count =
                        parse_positive_int(optarg, "starting_drones_count");
                break;

            case 'r':
                cfg->resupply_interval =
                        parse_positive_int(optarg, "resupply_interval");
                break;

            case 'c':
                cfg->maximum_charge_time =
                        parse_positive_int(optarg, "maximum_charge_time");
                break;

            case 'l':
                cfg->max_loading_cycles =
                        parse_positive_int(optarg, "max_loading_cycles");
                break;

            case 'g':
                cfg->gate_time_to_pass =
                        parse_positive_int(optarg, "gate_time_to_pass");
                break;

            case 'w':
                cfg->dron_work_interval =
                        parse_positive_int(optarg, "dron_work_interval");
                break;
            case 'm':
                maximum_drones_in_memory =
                        parse_positive_int(optarg, "maximum_drones_in_memory");
                break;

            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);

            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        exit(EXIT_FAILURE);
    }
}

void print_configuration(const SHM_Configuration *cfg) {
    if (cfg == NULL) {
        print_error("Configuration is NULL");
        return;
    }

    print_msg("=== Starting Configuration ===");

    print_msg("starting_drones_count    = %d", cfg->starting_drones_count);
    print_msg("resupply_interval        = %d", cfg->resupply_interval);
    print_msg("maximum_charge_time      = %d", cfg->maximum_charge_time);
    print_msg("max_loading_cycles       = %d", cfg->max_loading_cycles);
    print_msg("gate_time_to_pass        = %d", cfg->gate_time_to_pass);
    print_msg("dron_work_interval       = %d", cfg->dron_work_interval);
    print_msg("maximum_drones_in_memory = %d", maximum_drones_in_memory);
}

int creat_operator() {
    const pid_t pid = fork();
    if (pid < 0) {
        print_error("fork operator failed");
        return -1;
    }
    if (pid == 0) {
        sigset_t empty;

        if (sigemptyset(&empty) != 0) {
            print_error("sigemptyset failed");
            _exit(1);
        }

        if (sigprocmask(SIG_SETMASK, &empty, NULL) == -1) {
            print_error("sigprocmask failed");
            _exit(1);
        }
        execl("./operator", "./operator", (char *) NULL);

        print_error("exec operator failed");
        _exit(1);
    }

    operator_pid = pid;
    return 0;
}

int creat_system_commander() {
    const pid_t pid = fork();
    if (pid < 0) {
        print_error("fork system_commander failed");
        return -1;
    }
    if (pid == 0) {
        sigset_t empty;

        if (sigemptyset(&empty) != 0) {
            print_error("sigemptyset failed");
            _exit(1);
        }
        if (sigprocmask(SIG_SETMASK, &empty, NULL) == -1) {
            print_error("sigprocmask failed");
            _exit(1);
        }

        char op_pid_str[32];
        const int n = snprintf(op_pid_str, sizeof(op_pid_str), "%d", operator_pid);

        if (n < 0) {
            print_error("snprintf failed");
            _exit(1);
        }

        if (n >= (int) sizeof(op_pid_str)) {
            print_error("operator_pid string truncated");
            _exit(1);
        }

        execl("./system_commander", "./system_commander", op_pid_str, (char *) NULL);

        print_error("exec system_commander failed");
        _exit(1);
    }

    system_commander_pid = pid;
    return 0;
}

int create_shm_config(SHM_Configuration *local_configuration) {
    if (!local_configuration) return -1;

    const key_t key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (key < 0) {
        print_error("Cant grab key from file: %s", CONFIG_KEY_FILE_NAME);
        return -1;
    }

    shm_configuration_id = shm_create(key, sizeof(SHM_Configuration));
    if (shm_configuration_id == -1) {
        print_error("Cant create shm for configuration");
        return -1;
    }

    shm_configuration = shm_attach(shm_configuration_id);
    if (!shm_configuration) {
        print_error("Cant attach shm");
        return -1;
    }

    *shm_configuration = *local_configuration;

    shm_configuration_semaphore_id = semaphore_create(key, 1);
    if (shm_configuration_semaphore_id == -1) {
        print_error("Cant create semaphore for configuration");
        return -1;
    }

    return 0;
}

int create_shm_all_drones_data() {
    const key_t shm_all_drones_data_key = grab_key_from_file(ALL_DRONES_DATA_FILE_NAME);
    if (shm_all_drones_data_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    size_t bytes_needed = sizeof(SHM_AllDronesData) + maximum_drones_in_memory * sizeof(DronData);
    shm_all_drones_data_id = shm_create(shm_all_drones_data_key, bytes_needed);
    if (shm_all_drones_data_id < 0) {
        print_error("Failed to create shm for all_drones_data");
        return -1;
    }

    shm_all_drones_data = shm_attach(shm_all_drones_data_id);
    if (shm_all_drones_data == NULL) {
        print_error("Failed to attach shm for all_drones_data");
        return -1;
    }

    *shm_all_drones_data = (SHM_AllDronesData){
        .capacity = maximum_drones_in_memory,
        .next_dron_id = 0,
        .dron_in_base_count = 0,
        .maximum_dron_in_base_count = (STARTING_DRONE_COUNT_DEFAULT / 2) - 1,
        .drone_lost_count = 0,
        .dron_count = 0,
        .dron_reserving_space_count = 0
    };

    bytes_needed = Stack_bytes_needed(maximum_drones_in_memory, sizeof(int));

    const key_t shm_stack_key = grab_key_from_file(STACK_KEY_FILE_NAME);
    if (shm_stack_key < 0) {
        print_error("Cant grab key");
    }
    shm_stack_id = shm_create(shm_stack_key, bytes_needed);
    if (shm_stack_id < 0) {
        print_error("Failed to create shm for stack");
        return -1;
    }
    shm_stack = shm_attach(shm_stack_id);
    if (shm_stack == NULL) {
        print_error("Failed to attach shm for stack");
        return -1;
    }


    if (Stack_init(shm_stack, maximum_drones_in_memory, sizeof(int)) == STACK_ERROR) {
        print_error("Stack Failed with initialization");
        return -1;
    }

    int index = maximum_drones_in_memory - 1;
    while (!Stack_is_full(shm_stack)) {
        Stack_push(shm_stack, &index);
        index--;
    }

    shm_all_drones_data_semaphore_id = semaphore_create(shm_all_drones_data_key, 1);
    if (shm_all_drones_data_semaphore_id < 0) {
        print_error("Cant create semaphore for all_drones_data");
        return -1;
    }
    return 0;
}

int create_gate_semaphore() {
    const key_t gate_key = grab_key_from_file(GATE_KEY_FILE_NAME);
    if (gate_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    gate_semaphore_id = semaphore_create(gate_key, GATE_SEMAPHORE_STARTING_VALUE);
    if (gate_semaphore_id < 0) {
        print_error("Cant creat semaphore");
        return -1;
    }
    return 0;
}

int create_gate_fifo_sem() {
    if (fifo_sem_create(&fifo_sem,GATE_FIFO_FILE_NAME, 2) != 0) {
        print_error("Cant creat FIFO_SEM for gate");
        return -1;
    }
    return 0;
}

int close_main(const int exit_code) {
    int status;
    if (system_commander_pid > -1) {
        pid_t r = waitpid(system_commander_pid, &status, 0);

        if (r == -1) {
            print_error("waitpid system_commander failed");
        } else {
            print_msg("System_commander joined");

            if (WIFSIGNALED(status)) {
                print_error("System_commander killed by signal %d", WTERMSIG(status));
            } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                print_error("System_commander exited with code %d", WEXITSTATUS(status));
            }
        }
        system_commander_pid = -1;
    }
    if (operator_pid > -1) {
        pid_t r = waitpid(operator_pid, &status, 0);

        if (r == -1) {
            print_error("waitpid operator failed");
        } else {
            print_msg("Operator joined");

            if (WIFSIGNALED(status)) {
                print_error("Operator killed by signal %d", WTERMSIG(status));
            } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                print_error("Operator exited with code %d", WEXITSTATUS(status));
            }
        }

        operator_pid = -1;
    }
    if (shm_configuration_id > -1 && shm_configuration) {
        if (shm_detach(shm_configuration) != 0) {
            print_error("shm_detach configuration failed");
        }

        if (shm_destroy(shm_configuration_id) != 0) {
            print_error("shm_destroy configuration failed");
        }

        shm_configuration = NULL;
        shm_configuration_id = -1;
    }
    if (shm_all_drones_data_id > -1 && shm_all_drones_data) {
        if (shm_detach(shm_all_drones_data) != 0) {
            print_error("shm_detach all_drones_data failed");
        }

        if (shm_destroy(shm_all_drones_data_id) != 0) {
            print_error("shm_destroy all_drones_data failed");
        }

        shm_all_drones_data = NULL;
        shm_all_drones_data_id = -1;
    }
    if (shm_stack_id > -1 && shm_stack) {
        if (shm_detach(shm_stack) != 0) {
            print_error("shm_detach stack failed");
        }

        if (shm_destroy(shm_stack_id) != 0) {
            print_error("shm_destroy stack failed");
        }

        shm_stack = NULL;
        shm_stack_id = -1;
    }
    if (shm_configuration_semaphore_id > -1) {
        if (semaphore_delete(shm_configuration_semaphore_id) != 0) {
            print_error("semaphore_delete configuration failed");
        }
        shm_configuration_semaphore_id = -1;
    }
    if (shm_all_drones_data_semaphore_id > -1) {
        if (semaphore_delete(shm_all_drones_data_semaphore_id) != 0) {
            print_error("semaphore_delete all_drones_data failed");
        }

        shm_all_drones_data_semaphore_id = -1;
    }
    if (gate_semaphore_id > -1) {
        if (semaphore_delete(gate_semaphore_id) != 0) {
            print_error("semaphore_delete gate failed");
        }

        gate_semaphore_id = -1;
    }
    if (fifo_sem.file_descriptor > 0) {
        if (fifo_sem_destroy(&fifo_sem) != 0) {
            print_error("fifo_sem gate failed at destruction");
        }

        memset(&fifo_sem, 0, sizeof(fifo_sem));
        fifo_sem.file_descriptor = -1;
    }

    print_msg("Cleanup complete.");
    logger_shutdown();
    exit(exit_code);
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    signal(SIGINT, SIG_IGN);
    if (global_logger_initialize(LOG_FILE_NAME) != 0) {
        print_error("Global logger initialization failed");
        close_main(-1);
    }

    SHM_Configuration local_configuration = {
        .starting_drones_count = STARTING_DRONE_COUNT_DEFAULT,
        .resupply_interval = RESUPPLY_INTERVAL,
        .maximum_charge_time = MAXIMUM_CHARGE_TIME_DEFAULT,
        .max_loading_cycles = MAXIMUM_LOADING_CYCLES,
        .gate_time_to_pass = GATE_TIME_TO_PASS,
        .dron_work_interval = DRON_WORK_INTERVAL
    };
    process_argv(&local_configuration, argc, argv);
    print_configuration(&local_configuration);

    if (create_shm_config(&local_configuration) != 0) {
        print_error("Failed to creat shm configuration");
        close_main(-1);
    }

    if (create_shm_all_drones_data() != 0) {
        print_error("Failed to creat shm_all_drones_data");
        close_main(-1);
    }

    if (create_gate_semaphore() != 0) {
        print_error("Failed to creat gate_semaphore");
        close_main(-1);
    }

    if (create_gate_fifo_sem() != 0) {
        print_error("Failed to creat gate_fifo_sem");
        close_main(-1);
    }

    if (creat_operator() != 0) {
        print_error("Failed to creat operator process");
        close_main(-1);
    }

    if (creat_system_commander() != 0) {
        print_error("Failed to creat system commander process");
        close_main(-1);
    }

    print_msg("All setup complete, now waiting for children");
    close_main(0);

    return 0;
}
