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
    int id;
    DronData_Location location;
    int loading_cycles_left;
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

/*
 * No critical section is required because the file is used only as a stable anchor for ftok();
 * open(O_CREAT) is atomic and ftok() only reads file metadata without modifying it,
 * so concurrent calls produce the same key. */
key_t grab_key_from_file(const char *file_name);

int semaphore_create(const key_t key, const int semaphore_starting_value);

int semaphore_get(const key_t key);

int semaphore_delete(const int semaphore_id);


int shm_create(const key_t key, const size_t size);

int shm_get(const key_t key);

void *shm_attach(const int shm_id);

int shm_detach(const void *addr);

int shm_destroy(const int shm_id);


int SHM_AllDronesData_add_dron(SHM_AllDronesData *p_shm_all_drones_data, Stack *free_space_stack, DronData *p_dron_data);

int SHM_AllDronesData_mission_completed(SHM_AllDronesData *p_shm_all_drones_data);

int SHM_AllDronesData_update_dron_location(SHM_AllDronesData *p_shm_all_drones_data, int dron_index, DronData_Location new_dron_location);

int SHM_AllDronesData_delete_drone(SHM_AllDronesData *p_shm_all_drones_data, Stack *free_space_stack, int dron_index);
