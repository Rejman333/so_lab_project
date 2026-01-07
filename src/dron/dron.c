#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "ipc.h"
#include "printer.h"

#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define GATE_KEY_FILE_NAME "gate_key"
#define STACK_KEY_FILE_NAME "stack_key"
#define CONFIG_KEY_FILE_NAME "config_key"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

#define LOG_FILE_NAME "log.txt"

#define GATE_FIFO_FILE_NAME "/tmp/gate_fifo"

typedef struct {
    int my_id;
    int my_index;
    int maximum_charge_time;
    int loading_cycles_left;
    int have_reserved_space;
    int gate_time_to_pass;
    int work_interval;
    DronData_Location location;
} DronInternalData;

typedef struct {
    int charge_interval;
    int usage_interval;
} BatteryThreadArgs;

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_shutdown_requested = 0;

static volatile int is_charging = 0;
static volatile int battery_percentage = 100;

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

DronData_Location starting_location = LOCATION_UNDEFINE;
DronInternalData dron_internal_data = {0};
DronData dron_local_data = {0};

SHM_AllDronesData *shm_all_drones_data = NULL;
int shm_all_drones_data_id = -1;
Stack *shm_stack = NULL;
int shm_stack_id = -1;
int shm_all_drones_data_semaphore_id = -1;

pthread_t battery_thread = -1;
static volatile sig_atomic_t battery_working = 0;

int gate_semaphore_id = -1;

FIFO_SEM fifo_sem = {
    .file_descriptor = -1,
};


int close_main(const int exit_code) {
    if (shm_all_drones_data_id > -1 && shm_all_drones_data) {
        if (shm_detach(shm_all_drones_data) != 0) {
            print_error("shm_detach all_drones_data failed");
        }

        shm_all_drones_data = NULL;
        shm_all_drones_data_id = -1;
    }
    if (shm_stack_id > -1 && shm_stack) {
        if (shm_detach(shm_stack) != 0) {
            print_error("shm_detach stack failed");
        }

        shm_stack = NULL;
        shm_stack_id = -1;
    }
    if (fifo_sem.file_descriptor > 0) {
        if (fifo_sem_close(&fifo_sem) != 0) {
            print_error("Failed to close fifo_sem");
        }

        memset(&fifo_sem, 0, sizeof(fifo_sem));
        fifo_sem.file_descriptor = -1;
    }

    shm_all_drones_data_semaphore_id = -1;
    gate_semaphore_id = -1;

    if (battery_working > 0) {
        battery_working = 0;
        if (pthread_join(battery_thread, NULL) != 0) {
            print_error("pthread_join failed");
        }
    }

    print_msg("Cleanup complete.");
    logger_shutdown();
    exit(exit_code);
}

void print_dron_internal_data(const DronInternalData *dron) {
    if (!dron) {
        print_error("print_dron_internal_data: dron is NULL");
        return;
    }

    print_msg("=== Dron Internal Data ===");

    print_msg("my_id                 = %d", dron->my_id);
    print_msg("my_index              = %d", dron->my_index);
    print_msg("maximum_charge_time   = %d", dron->maximum_charge_time);
    print_msg("loading_cycles_left   = %d", dron->loading_cycles_left);
    print_msg("have_reserved_space   = %d", dron->have_reserved_space);
    print_msg("gate_time_to_pass     = %d", dron->gate_time_to_pass);
    print_msg("work_interval         = %d", dron->work_interval);
    print_msg("location              = %s", DronData_LocationToString(dron->location));
}

void shutdown_request_handler(int sig) {
    got_shutdown_requested = 1;
}

void sig_suicide_handler(int sig) {
    got_sigusr1 = 1;
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
        return -1;
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

int get_gate_semaphore() {
    const key_t gate_key = grab_key_from_file(GATE_KEY_FILE_NAME);
    if (gate_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    gate_semaphore_id = semaphore_get(gate_key);
    if (gate_semaphore_id < 0) {
        print_error("Cant get semaphore");
        return -1;
    }
    return 0;
}

int get_initial_configuration() {
    DronInternalData new_dron_internal_data = {0};
    const key_t shm_config_key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (shm_config_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_config_id = shm_get(shm_config_key);
    if (shm_config_id < 0) {
        print_error("Cant get shm config");
        return -1;
    }

    const int shm_config_semaphore_id = semaphore_get(shm_config_key);
    if (shm_config_semaphore_id < 0) {
        print_error("Cant get semaphore for config");
        return -1;
    }

    SHM_Configuration *p_shm_config = shm_attach(shm_config_id);
    if (!p_shm_config) {
        print_error("Cant attach shm config");
        return -1;
    }

    if (semaphore_lock(shm_config_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        if (shm_detach(p_shm_config) != 0) {
            print_error("shm_detach config failed");
        }
        return -1;
    }

    new_dron_internal_data.maximum_charge_time = p_shm_config->maximum_charge_time;
    new_dron_internal_data.loading_cycles_left = p_shm_config->max_loading_cycles;
    new_dron_internal_data.gate_time_to_pass = p_shm_config->gate_time_to_pass;
    new_dron_internal_data.work_interval = p_shm_config->dron_work_interval;

    if (semaphore_unlock(shm_config_semaphore_id) == -1) {
        print_error("While leaving semaphore: semop +1");
        if (shm_detach(p_shm_config) != 0) {
            print_error("shm_detach config failed");
        }
        return -1;
    }

    if (shm_detach(p_shm_config) != 0) {
        print_error("shm_detach config failed");
        return -1;
    }

    new_dron_internal_data.my_index = -1;
    new_dron_internal_data.location = starting_location;
    new_dron_internal_data.have_reserved_space = 0;

    dron_internal_data = new_dron_internal_data;

    if (starting_location == LOCATION_MISSION) {
        is_charging = 0;
    } else {
        is_charging = 1;
    }

    return 0;
}

int get_gate_fifo_sem() {
    if (fifo_sem_get(&fifo_sem,GATE_FIFO_FILE_NAME) != 0) {
        print_error("Cant get FIFO_SEM for gate");
        return -1;
    }
    return 0;
}


void *battery(void *arg) {
    const BatteryThreadArgs *battery_thread_args = (BatteryThreadArgs *) arg;
    battery_working = 1;
    while (battery_working) {
        pthread_mutex_lock(&m);

        if (is_charging) {
            if (battery_percentage < 100) battery_percentage++;
        } else {
            if (battery_percentage > 0) battery_percentage--;
        }

        pthread_mutex_unlock(&m);

        if (is_charging) usleep(battery_thread_args->charge_interval);
        else usleep(battery_thread_args->usage_interval);
    }
    return NULL;
}

int pass_the_gate() {
    if (semaphore_lock(gate_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    print_msg_color(COLOR_RED, "Entered the gate");
    usleep(dron_internal_data.gate_time_to_pass);
    print_msg_color(COLOR_RED, "Left the gate");

    if (semaphore_unlock(gate_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }
    return 0;
}

int pass_the_gate_fifo() {
    if (fifo_sem_lock(&fifo_sem) == -1) {
        print_error("Error while waiting for fifi_sem");
        return -1;
    }

    print_msg_color(COLOR_RED, "Entered the gate");
    usleep(dron_internal_data.gate_time_to_pass);
    print_msg_color(COLOR_RED, "Left the gate");

    if (fifo_sem_unlock(&fifo_sem) == -1) {
        print_error("Error while unlocking fifo");
        return -1;
    }
    return 0;
}

int process_sigusr1() {
    got_sigusr1 = 0;
    pthread_mutex_lock(&m);
    const int current_battery_percentage = battery_percentage;
    pthread_mutex_unlock(&m);

    if (current_battery_percentage > 20) {
        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, dron_internal_data.my_index,
                                       dron_internal_data.have_reserved_space);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        print_msg_color(COLOR_RED, "Dron with id: %d Suicided", dron_internal_data.my_id);
        close_main(EXIT_SUCCESS);
    }
    print_msg_color(COLOR_YELLOW, "Dron with id: %d ignored suicide command battery at: %d",
                    dron_internal_data.my_id, current_battery_percentage);

    return 0;
}

int battery_state_check() {
    pthread_mutex_lock(&m);
    const int current_battery_percentage = battery_percentage;
    pthread_mutex_unlock(&m);

    if (current_battery_percentage <= 0) {
        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, dron_internal_data.my_id,
                                       dron_internal_data.have_reserved_space);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        print_msg_color(COLOR_RED, "Dron with id: %d Out of power", dron_internal_data.my_id);
        close_main(EXIT_SUCCESS);
    }
    return 0;
}

int force_base_return() {
    pthread_mutex_lock(&m);
    const int current_battery_percentage = battery_percentage;
    pthread_mutex_unlock(&m);

    if (current_battery_percentage < 20) {
        if (dron_internal_data.loading_cycles_left <= 0) {
            if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, dron_internal_data.my_index,
                                           dron_internal_data.have_reserved_space);

            if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            print_msg_color(COLOR_RED, "Dron with id: %d Deactivated", dron_internal_data.my_id);
            close_main(EXIT_SUCCESS);
        }

        print_msg_color(COLOR_YELLOW, "Going for a charge");

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        if (shm_all_drones_data->dron_in_base_count + shm_all_drones_data->dron_reserving_space_count <
            shm_all_drones_data->maximum_dron_in_base_count) {
            shm_all_drones_data->dron_reserving_space_count++;
            dron_internal_data.have_reserved_space = 1;
            print_msg("Reserved space");
        } else {
            print_msg("Failed to reserve space");
            if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }
            return 0;
        }

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        // if (pass_the_gate() != 0) {
        //     print_error("Error while passing the gate");
        //
        //     return -1;
        // };

        if (pass_the_gate_fifo() != 0) {
            print_error("Error while passing the gate");

            return -1;
        };

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_update_dron_location(shm_all_drones_data, dron_internal_data.my_index, LOCATION_BASE);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }
        dron_internal_data.have_reserved_space = 0;
        dron_internal_data.loading_cycles_left--;
        dron_internal_data.location = LOCATION_BASE;
        is_charging = 1;
    }

    return 0;
}

int force_leave_base() {
    pthread_mutex_lock(&m);
    const int current_battery_percentage = battery_percentage;
    pthread_mutex_unlock(&m);


    if (dron_internal_data.location == LOCATION_BASE && current_battery_percentage >= 100) {
        print_msg_color(COLOR_YELLOW, "Going for a mission");

        pass_the_gate(gate_semaphore_id, dron_internal_data.gate_time_to_pass);

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_update_dron_location(shm_all_drones_data, dron_internal_data.my_index, LOCATION_MISSION);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }
        dron_internal_data.location = LOCATION_MISSION;
        is_charging = 0;
    }
    return 0;
}

void describe_self() {
    pthread_mutex_lock(&m);
    const int current_battery_percentage = battery_percentage;
    pthread_mutex_unlock(&m);

    print_msg(
        "ID: %3d | Battery: %3d%%[%2d] | %s",
        dron_internal_data.my_id,
        current_battery_percentage,
        dron_internal_data.loading_cycles_left,
        DronData_LocationToString(dron_internal_data.location)
    );
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

void process_argv_location(int argc, char *argv[]) {
    if (argc < 2) {
        print_error("Dron location not specified");
        exit(EXIT_FAILURE);
    }

    starting_location = parse_positive_int(argv[1], "location");
}

int try_adding_self_to_shm() {
    if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    if (shm_all_drones_data->dron_in_base_count + shm_all_drones_data->dron_reserving_space_count < shm_all_drones_data
        ->maximum_dron_in_base_count) {
        dron_internal_data.my_index = SHM_AllDronesData_add_dron(shm_all_drones_data, shm_stack, &dron_local_data);
        dron_internal_data.my_id = shm_all_drones_data->next_dron_id++;
        if (dron_internal_data.my_index == -1) {
            print_msg_color(COLOR_YELLOW, "Max drones in memory reached.");
            return -1;
        }
    } else {
        print_msg_color(COLOR_YELLOW, "Max drones in base reached.");
        return -1;
    }


    if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    if (logger_initialize(LOG_FILE_NAME) != 0) {
        print_error("Logger initialization failed");
        close_main(EXIT_FAILURE);
    }

    process_argv_location(argc, argv);


    struct sigaction sa_shutdown = {0};
    sa_shutdown.sa_handler = shutdown_request_handler;

    sigfillset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa_shutdown, NULL) == -1) {
        print_error("sigaction(SIGINT) failed");
        close_main(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa_shutdown, NULL) == -1) {
        print_error("sigaction(SIGTERM) failed");
        close_main(EXIT_FAILURE);
    }


    struct sigaction sa_suicide = {0};
    sa_suicide.sa_handler = sig_suicide_handler;

    sigfillset(&sa_suicide.sa_mask);
    sa_suicide.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa_suicide, NULL) == -1) {
        print_error("sigaction(SIGUSR1) failed");
        close_main(EXIT_FAILURE);
    }


    if (get_initial_configuration() == -1) {
        print_error("Failed to initiate configuration");
        close_main(EXIT_FAILURE);
    }

    if (get_shm_all_drones_data() != 0) {
        print_error("Failed to get/attach shm/semaphore");
        close_main(EXIT_FAILURE);
    }


    if (get_gate_semaphore() != 0) {
        print_error("Failed to gate gate_semaphore");
        close_main(EXIT_FAILURE);
    }

    if (get_gate_fifo_sem() != 0) {
        print_error("Failed to gate gate_fifi_sem");
        close_main(EXIT_FAILURE);
    }


    dron_local_data.id = dron_internal_data.my_id;
    dron_local_data.location = starting_location;
    dron_local_data.pid = getpid();

    BatteryThreadArgs battery_thread_args = {
        .charge_interval = dron_internal_data.maximum_charge_time / 100,
        .usage_interval = (int) ((dron_internal_data.maximum_charge_time * 2.5) / 100.)
    };

    if (pthread_create(&battery_thread, NULL, battery, &battery_thread_args)) {
        print_error("pthread_create failed");
        close_main(EXIT_FAILURE);
    }

    if (try_adding_self_to_shm() != 0) {
        close_main(EXIT_FAILURE);
    }

    print_msg("Started with id: %d, with index of: %d", dron_internal_data.my_id, dron_internal_data.my_index);


    while (!got_shutdown_requested) {
        if (got_sigusr1) {
            if (process_sigusr1() != 0) {
                print_error("Processing sigusr1 failed");
                close_main(EXIT_FAILURE);
            }
        }


        if (battery_state_check() != 0) {
            print_error("Battery check failed");
            close_main(EXIT_FAILURE);
        }


        if (dron_internal_data.location == LOCATION_MISSION) {
            if (force_base_return() != 0) {
                print_error("Failed base return");
                close_main(EXIT_FAILURE);
            }
        } else if (dron_internal_data.location == LOCATION_BASE) {
            if (force_leave_base() != 0) {
                print_error("Failed base return");
                close_main(EXIT_FAILURE);
            }
        }

        describe_self();
        usleep(dron_internal_data.work_interval);
    }

    close_main(EXIT_SUCCESS);
    return 0;
}
