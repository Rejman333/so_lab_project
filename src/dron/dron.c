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

typedef struct {
    int my_id;
    int my_index;
    int maximum_charge_time;
    int loading_cycles_left;
    int mission_time;
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


void sig_end_handler(int sig) {
    got_shutdown_requested = 1;
}

void sig_suicide_handler(int sig) {
    got_sigusr1 = 1;
}

int get_random_mission_time(const int min, const int max) {
    return min + rand() % (max - min + 1);
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

    if (semop(shm_config_semaphore_id, &SEM_LOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    out_config->maximum_charge_time = p_shm_config->maximum_charge_time;
    out_config->loading_cycles_left = p_shm_config->max_loading_cycles;
    out_config->my_id = p_shm_config->next_dron_id++;

    if (semop(shm_config_semaphore_id, &SEM_UNLOCK, 1) == -1) {
        print_error("While leaving semaphore: semop +1");
        return -1;
    }
    shm_detach(p_shm_config);
    out_config->my_index = -1;

    if (starting_location == LOCATION_MISSION) {
        out_config->mission_time = get_random_mission_time(5000, 15000);
        is_charging = 0;
    } else {
        is_charging = 1;
    };
    out_config->location = starting_location;

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


int main(int argc, char *argv[]) {
    setup_print(PROCESS_NAME, PROCESS_COLOR);
    srand(time(NULL));

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


    struct sigaction sig_end;
    sig_end.sa_handler = sig_end_handler;
    sigfillset(&sig_end.sa_mask);
    sig_end.sa_flags = 0;
    sigaction(SIGTERM, &sig_end, NULL);

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

    if (semop(shm_all_drones_data_semaphore_id, &SEM_LOCK, 1) == -1) {
        print_error("While waiting for semaphore: semop -1");
        return -1;
    }

    my_data.my_index = SHM_AllDronesData_add_dron(shm_all_drones_data, shm_stack, &my_shm_data);
    if (my_data.my_index == -1) {
        //ToDo
        if (semop(shm_all_drones_data_semaphore_id, &SEM_UNLOCK, 1) == -1) {
            print_error("While waiting for semaphore: semop -1");
        }
        print_msg_color(COLOR_YELLOW, "Max drones in memory reached, i am deleting myself");
        exit(1);
    }

    if (semop(shm_all_drones_data_semaphore_id, &SEM_UNLOCK, 1) == -1) {
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
            got_sigusr1 = 0;
            pthread_mutex_lock(&m);
            if (battery_percentage > 20) {
                if (semop(shm_all_drones_data_semaphore_id, &SEM_LOCK, 1) == -1) {
                    print_error("While waiting for semaphore: semop -1");
                    return -1;
                }

                SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, my_data.my_id);
                if (semop(shm_all_drones_data_semaphore_id, &SEM_UNLOCK, 1) == -1) {
                    print_error("While waiting for semaphore: semop -1");
                    return -1;
                }

                print_msg_color(COLOR_RED, "Dron with id: %d Suicided", my_data.my_id);
                return (0);
            }
            print_msg_color(COLOR_YELLOW, "Dron with id: %d ignored suicide command battery at: %d",
                            my_data.my_id, battery_percentage);
            pthread_mutex_unlock(&m);
        }
        pthread_mutex_lock(&m);
        if (battery_percentage <= 0) {
            if (semop(shm_all_drones_data_semaphore_id, &SEM_LOCK, 1) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, my_data.my_id);
            if (semop(shm_all_drones_data_semaphore_id, &SEM_UNLOCK, 1) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            print_msg_color(COLOR_RED, "Dron with id: %d Out of power", my_data.my_id);
            return (0);
        }
        pthread_mutex_unlock(&m);

        pthread_mutex_lock(&m);
        if (my_data.location == LOCATION_MISSION && battery_percentage < 20) {
            print_msg_color(COLOR_YELLOW, "Going for a charge");

            if (semop(shm_all_drones_data_semaphore_id, &SEM_LOCK, 1) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            SHM_AllDronesData_update_dron_location(shm_all_drones_data,my_data.my_index, LOCATION_BASE);
            if (semop(shm_all_drones_data_semaphore_id, &SEM_UNLOCK, 1) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }
            my_data.location = LOCATION_BASE;
            is_charging = 1;

        }
        pthread_mutex_unlock(&m);

        pthread_mutex_lock(&m);
        if (my_data.location == LOCATION_BASE && battery_percentage >= 100) {
            print_msg_color(COLOR_YELLOW, "Going for a mission");

            if (semop(shm_all_drones_data_semaphore_id, &SEM_LOCK, 1) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }

            SHM_AllDronesData_update_dron_location(shm_all_drones_data,my_data.my_index, LOCATION_MISSION);
            if (semop(shm_all_drones_data_semaphore_id, &SEM_UNLOCK, 1) == -1) {
                print_error("While waiting for semaphore: semop -1");
                return -1;
            }
            my_data.location = LOCATION_MISSION;
            is_charging = 0;
            my_data.mission_time = get_random_mission_time(5000, 15000);
        }
        pthread_mutex_unlock(&m);



        pthread_mutex_lock(&m);
        print_msg(
            "ID: %3d | Battery: %3d%% | Location: %3d | Mission: %3d%%",
            my_data.my_id,
            battery_percentage,
            my_data.location,
            0
        );
        pthread_mutex_unlock(&m);
        sleep(1);
    }
    return 0;
}

// Jasiek jedna kwestia dodaj funkcje pass_the_gate ogarnij mutex semafore hell -> powodzenia
// Cas misji licz jako początek czasu + czas misji = o tej godzinie kończysz, i to testujesz nie trzeba sleep;
// Albo jakiś sprytneijszy sposób->ogarnać sprytneijsze czekania
// Dodać semafor na wejście do bazy i wyjście z bazy coś jak z gatem
// Potem to już głównie testy, iejszcze raz testy Ale ten dynamicznie smienainy semafor w bazie może być trudny wiec może shm będzie lepsze
// Wyłączanie symulacji gdy ilość dronuw w akcji == 0
// Dodać śmierć przez zurzycie bateri maksymalna ilosć łądowań
// Dodać stan lokalny na nim operujemy, a potem tylko aktualizujemy stan globalny, przez mem copy
// Dodać nową lokację w kolejce do bazy i w kolejce na misje

