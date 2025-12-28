#pragma once
#include <sys/types.h>
#include <sys/sem.h>

#include "stack.h"

typedef struct {
    int starting_drones_count;
    int maximum_drones_count;
    int resupply_interval;
    int maximum_charge_time;
    int max_loading_cycles;
} SHM_Configuration;

typedef enum {
    LOCATION_UNDEFINE,
    LOCATION_BASE,
    LOCATION_MISSION
} DronData_Location;

typedef struct {
    pid_t pid;
    DronData_Location dron_location;
    int loading_cycles_left;
    time_t last_update;
} DronData;

typedef struct {
    int dron_count;
    int dron_in_base_count;
    int missions_completed_count;
    int drone_lost_count;

    DronData drones[];
} SHM_AllDronesData;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};


#define SEM_LOCK   ((struct sembuf){0, -1, 0})
#define SEM_UNLOCK ((struct sembuf){0, +1, 0})

key_t grab_key_from_file(const char *file_name);

int create_semaphore(const key_t key, int semaphore_starting_value);

int get_semaphore(const key_t key);

int delete_semaphore(int semaphore_id);


int shm_create(key_t key, size_t size);

int shm_open_existing(key_t key);

void *shm_attach(int shmid);

int shm_detach(const void *addr);

int shm_destroy(int shmid);

int SHM_DronInfo_add_dron(SHM_AllDronesData *p_shm_dron_info, Stack *free_space_stack, DronData *p_dron_state);

int SHM_DronInfo_mission_completed(SHM_AllDronesData *p_shm_dron_info);

int SHM_DronInfo_update_dron_location(SHM_AllDronesData *p_shm_dron_info, int dron_index, DronData_Location new_dron_location);

int SHM_DronInfo_delete_drone(SHM_AllDronesData *p_shm_dron_info, Stack *free_space_stack, int dron_index);
