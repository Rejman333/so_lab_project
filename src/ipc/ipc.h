#pragma once
#include <sys/types.h>

#include "printer.h"

// Types for tpc defined

typedef struct {
    int starting_drones_count;
    int resupply_interval;
    int maximum_charge_time;
    int max_loading_cycles;
} SHM_Configuration;

typedef enum {
    LOCATION_UNDEFINE,
    LOCATION_BASE,
    LOCATION_MISSION
} Location;

typedef struct {
    pid_t pid;
    Location dron_location;
    int loading_cycles_left;
    time_t last_update;
} Dron_State;

typedef struct {
    int dron_count;
    int dron_in_base_count;
    int missions_completed_count;
    int drone_lost_count;
} SHM_DronInfo;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

key_t grab_key_from_file(const char *file_name);

int create_semaphore(const char *file_name, int semaphore_starting_value);

int get_semaphore(const char *file_name);

void delete_semaphore(int semaphore_id);


int shm_create(key_t key, size_t size);

int shm_open_existing(key_t key);

void *shm_attach(int shmid);

int shm_detach(const void *addr);

int shm_destroy(int shmid);

int shm_get_size(int shmid);

int shm_lock(int shmid);

int shm_unlock(int shmid);
