#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/sem.h>

#include "ipc.h"
#include "printer.h"

#define ALL_DRONES_DATA_FILE_NAME "dron_info_key"
#define GATE_KEY_FILE_NAME "gate_key"
#define STACK_KEY_FILE_NAME "stack_key"
#define CONFIG_KEY_FILE_NAME "config_key"

#define PROCESS_NAME "Dron"
#define PROCESS_COLOR COLOR_MAGENTA

#define LOG_FILE_NAME "log.txt"

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


void shutdown_request_handler(int sig) {
    got_shutdown_requested = 1;
}

void sig_suicide_handler(int sig) {
    got_sigusr1 = 1;
}

int get_shm_all_drones_data(SHM_AllDronesData **out_data, Stack **out_stack, int *out_stack_id, int *out_sem_id) {
    if (!out_data || !out_sem_id) return -1;

    const key_t shm_all_drones_data_key = grab_key_from_file(ALL_DRONES_DATA_FILE_NAME);
    if (shm_all_drones_data_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_all_drones_data_id = shm_get(shm_all_drones_data_key);
    if (shm_all_drones_data_id < 0) {
        print_error("Cant open shm");
        return -1;
    }

    SHM_AllDronesData *p_shm_all_drones_data = shm_attach(shm_all_drones_data_id);

    const key_t shm_stack_key = grab_key_from_file(STACK_KEY_FILE_NAME);
    if (shm_stack_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_stack_id = shm_get(shm_stack_key);
    if (shm_stack_id < 0) {
        print_error("Cant open shm");
        return -1;
    }
    Stack *stack = shm_attach(shm_stack_id);


    *out_sem_id = semaphore_get(shm_all_drones_data_key);

    *out_data = p_shm_all_drones_data;
    *out_stack = stack;
    *out_stack_id = shm_stack_id;

    return shm_all_drones_data_id;
}

int get_gate_semaphore() {
    const key_t gate_key = grab_key_from_file(GATE_KEY_FILE_NAME);
    if (gate_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int gate_semaphore_id = semaphore_get(gate_key);
    return gate_semaphore_id;
}

int get_initial_configuration(DronInternalData *out_config, const DronData_Location starting_location) {
    const key_t shm_config_key = grab_key_from_file(CONFIG_KEY_FILE_NAME);
    if (shm_config_key < 0) {
        print_error("Cant grab key");
        return -1;
    }

    const int shm_config_id = shm_get(shm_config_key);
    if (shm_config_id < 0) {
        print_error("Cant open shm");
        return -1;
    }

    SHM_Configuration *p_shm_config = shm_attach(shm_config_id);
    const int shm_config_semaphore_id = semaphore_get(shm_config_key);

    if (semaphore_lock(shm_config_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    out_config->maximum_charge_time = p_shm_config->maximum_charge_time;
    out_config->loading_cycles_left = p_shm_config->max_loading_cycles;
    out_config->gate_time_to_pass = p_shm_config->gate_time_to_pass;
    out_config->work_interval = p_shm_config->dron_work_interval;

    if (semaphore_unlock(shm_config_semaphore_id) == -1) {
        print_error("While leaving semaphore: semop +1");
        return -1;
    }
    shm_detach(p_shm_config);
    out_config->my_index = -1;

    if (starting_location == LOCATION_MISSION) {
        is_charging = 0;
    } else {
        is_charging = 1;
    };
    out_config->location = starting_location;
    out_config->have_reserved_space = 0;

    return 0;
}

void *battery(void *arg) {
    const BatteryThreadArgs *battery_thread_args = (BatteryThreadArgs *) arg;
    while (1) {
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
}

int pass_the_gate(const int gate_semaphore_id, int gate_time_to_pass) {
    if (semaphore_lock(gate_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    print_msg_color(COLOR_RED,"Entered the gate");
    usleep(gate_time_to_pass);
    print_msg_color(COLOR_RED, "Left the gate");

    if (semaphore_unlock(gate_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }
    return 0;
}

int process_sigusr1(SHM_AllDronesData *shm_all_drones_data, Stack *shm_stack, DronInternalData *my_data,
                    const int shm_all_drones_data_semaphore_id) {
    got_sigusr1 = 0;
    pthread_mutex_lock(&m);
    if (battery_percentage > 20) {
        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, my_data->my_index, my_data->have_reserved_space);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        print_msg_color(COLOR_RED, "Dron with id: %d Suicided", my_data->my_id);
        exit(0);
    }
    print_msg_color(COLOR_YELLOW, "Dron with id: %d ignored suicide command battery at: %d",
                    my_data->my_id, battery_percentage);
    pthread_mutex_unlock(&m);
    return 0;
}

int battery_state_check(SHM_AllDronesData *shm_all_drones_data, Stack *shm_stack, DronInternalData *my_data,
                        const int shm_all_drones_data_semaphore_id) {
    pthread_mutex_lock(&m);
    if (battery_percentage <= 0) {
        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, my_data->my_id, my_data->have_reserved_space);
        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        print_msg_color(COLOR_RED, "Dron with id: %d Out of power", my_data->my_id);
        exit(0);
    }
    pthread_mutex_unlock(&m);
    return 0;
}

int force_base_return(SHM_AllDronesData *shm_all_drones_data, Stack *shm_stack, DronInternalData *my_data,
                      const int shm_all_drones_data_semaphore_id, const int gate_semaphore_id) {

    pthread_mutex_lock(&m);
    int current_battery_percentage = battery_percentage;
    pthread_mutex_unlock(&m);

    if (current_battery_percentage < 20) {
        if (my_data->loading_cycles_left <= 0) {
            if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, my_data->my_index, my_data->have_reserved_space);

            if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            print_msg_color(COLOR_RED, "Dron with id: %d Deactivated", my_data->my_id);
            exit(0);
        }

        print_msg_color(COLOR_YELLOW, "Going for a charge");

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        if (shm_all_drones_data->dron_in_base_count + shm_all_drones_data->dron_reserving_space_count <
            shm_all_drones_data->maximum_dron_in_base_count) {
            shm_all_drones_data->dron_reserving_space_count ++;
            my_data->have_reserved_space = 1;
            print_msg("Reserved space");

        }else {
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

        pass_the_gate(gate_semaphore_id, my_data->gate_time_to_pass);

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_update_dron_location(shm_all_drones_data, my_data->my_index, LOCATION_BASE);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }
        my_data->have_reserved_space = 0;
        my_data->loading_cycles_left--;
        my_data->location = LOCATION_BASE;
        is_charging = 1;
    }

    return 0;
}

int force_leave_base(SHM_AllDronesData *shm_all_drones_data, DronInternalData *my_data,
                     const int shm_all_drones_data_semaphore_id, const int gate_semaphore_id) {
    pthread_mutex_lock(&m);
    if (my_data->location == LOCATION_BASE && battery_percentage >= 100) {
        print_msg_color(COLOR_YELLOW, "Going for a mission");



        pass_the_gate(gate_semaphore_id, my_data->gate_time_to_pass);

        if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }

        SHM_AllDronesData_update_dron_location(shm_all_drones_data, my_data->my_index, LOCATION_MISSION);

        if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
            print_error("While waiting for semaphore: semop -1");
            return -1;
        }
        my_data->location = LOCATION_MISSION;
        is_charging = 0;
    }
    pthread_mutex_unlock(&m);
    return 0;
}

void describe_self(const DronInternalData *my_data) {
    pthread_mutex_lock(&m);
    print_msg(
        "ID: %3d | Battery: %3d%%[%2d] | %s",
        my_data->my_id,
        battery_percentage,
        my_data->loading_cycles_left,
        DronData_LocationToString(my_data->location)
    );
    pthread_mutex_unlock(&m);
}


int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    logger_initialize(LOG_FILE_NAME);

    if (argc < 2) {
        print_error("No location argument");
        exit(1);
    }
    const DronData_Location starting_location = atoi(argv[1]);


    DronInternalData my_data;
    if (get_initial_configuration(&my_data, starting_location) == -1) {
        print_error("Failed to initiate configuration");
    }

    SHM_AllDronesData *shm_all_drones_data = NULL;
    Stack *shm_stack = NULL;
    int shm_stack_id = -1;
    int shm_all_drones_data_semaphore_id = -1;

    const int shm_all_drones_data_id = get_shm_all_drones_data(&shm_all_drones_data,
                                                               &shm_stack,
                                                               &shm_stack_id,
                                                               &shm_all_drones_data_semaphore_id);


    if (shm_all_drones_data_id == EXIT_FAILURE) {
        // Todo handle error
    }

    int gate_semaphore_id = get_gate_semaphore();


    struct sigaction sig_shutdown_request;
    sig_shutdown_request.sa_handler = shutdown_request_handler;
    sigfillset(&sig_shutdown_request.sa_mask);
    sig_shutdown_request.sa_flags = 0;
    sigaction(SIGINT, &sig_shutdown_request, NULL);
    sigaction(SIGTERM, &sig_shutdown_request, NULL);

    struct sigaction sif_suicide;
    sif_suicide.sa_handler = sig_suicide_handler;
    sigfillset(&sif_suicide.sa_mask);
    sif_suicide.sa_flags = 0;
    sigaction(SIGUSR1, &sif_suicide, NULL);

    DronData my_shm_data = {
        .id = my_data.my_id,
        .location = starting_location,
        .pid = getpid()
    };

    if (semaphore_lock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    if (shm_all_drones_data->dron_in_base_count + shm_all_drones_data->dron_reserving_space_count < shm_all_drones_data
        ->maximum_dron_in_base_count) {
        my_data.my_index = SHM_AllDronesData_add_dron(shm_all_drones_data, shm_stack, &my_shm_data);
        my_data.my_id = shm_all_drones_data->next_dron_id++;
        if (my_data.my_index == -1) {
            print_msg_color(COLOR_YELLOW, "Max drones in memory reached.");
            exit(1);
        }
    } else {
        print_msg_color(COLOR_YELLOW, "Max drones in base reached.");
        exit(1);
    }


    if (semaphore_unlock(shm_all_drones_data_semaphore_id) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }


    BatteryThreadArgs battery_thread_args = {
        .charge_interval = my_data.maximum_charge_time / 100,
        .usage_interval = (int) ((my_data.maximum_charge_time * 2.5) / 100.)
    };
    pthread_t battery_thread;
    pthread_create(&battery_thread, NULL, battery, &battery_thread_args);

    print_msg("Started with id: %d, with index of: %d", my_data.my_id, my_data.my_index);

    while (!got_shutdown_requested) {
        if (got_sigusr1) {
            process_sigusr1(shm_all_drones_data, shm_stack, &my_data, shm_all_drones_data_semaphore_id);
        }

        battery_state_check(shm_all_drones_data, shm_stack, &my_data, shm_all_drones_data_semaphore_id);


        if (my_data.location == LOCATION_MISSION) {
            force_base_return(shm_all_drones_data, shm_stack, &my_data, shm_all_drones_data_semaphore_id,
                              gate_semaphore_id);
        }

        if (my_data.location == LOCATION_BASE) {
            force_leave_base(shm_all_drones_data, &my_data, shm_all_drones_data_semaphore_id, gate_semaphore_id);
        }

        describe_self(&my_data);
        usleep(my_data.work_interval);
    }
    print_msg("Exiting");
    logger_shutdown();
    return 0;
}

