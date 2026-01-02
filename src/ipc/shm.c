#include <string.h>

#include "ipc.h"

#include <unistd.h>
#include <sys/shm.h>

#include "printer.h"

int shm_create(const key_t key, const size_t size) {
    const int shm_id = shmget(key, size, 0600 | IPC_CREAT | IPC_EXCL);
    if (shm_id == -1) {
        print_error("Failed to create sheared memory");
        return -1;
    }
    return shm_id;
};

int shm_get(const key_t key) {
    const int shm_id = shmget(key, 0, 0600);
    if (shm_id == -1) {
        print_error("Failed to open sheared memory");
        return -1;
    }
    return shm_id;
}

void *shm_attach(const int shm_id) {
    void *addr = shmat(shm_id, NULL, 0);

    if (addr == (void *) -1) {
        print_error("Failed to attach sheared memory");
        return NULL;
    }

    return addr;
}

int shm_detach(const void *addr) {
    if (shmdt(addr) == -1) {
        print_error("Failed to detach sheared memory");
        return -1;
    }
    return 0;
}

int shm_destroy(const int shm_id) {
    // Mark for deletion
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        print_error("Failed to destroy sheared memory");
        return -1;
    }
    return 0;
}

int SHM_AllDronesData_add_dron(SHM_AllDronesData *p_shm_all_drones_data, Stack *free_space_stack,
                               DronData *p_dron_data) {
    int out = -1;
    if (Stack_pop(free_space_stack, &out) == STACK_ERROR) return -1;

    p_shm_all_drones_data->dron_count++;

    if (p_dron_data->location == LOCATION_BASE) p_shm_all_drones_data->dron_in_base_count++;

    memcpy(&p_shm_all_drones_data->drones[out], p_dron_data, sizeof(DronData));

    return out;
};


int SHM_AllDronesData_update_dron_location(SHM_AllDronesData *p_shm_all_drones_data, const int dron_index,
                                           DronData_Location new_dron_location) {
    const DronData_Location old_location = p_shm_all_drones_data->drones[dron_index].location;
    if (old_location == LOCATION_BASE) p_shm_all_drones_data->dron_in_base_count--;
    if (old_location == LOCATION_MISSION) {
        p_shm_all_drones_data->dron_in_base_count++;
        p_shm_all_drones_data->dron_reserving_space_count--;
    }
    p_shm_all_drones_data->drones[dron_index].location = new_dron_location;
    return 0;
};

int SHM_AllDronesData_delete_drone(SHM_AllDronesData *p_shm_all_drones_data, Stack *free_space_stack,
                                   const int dron_index, const int has_space_reserved) {
    p_shm_all_drones_data->dron_count--;
    p_shm_all_drones_data->drone_lost_count++;
    if (p_shm_all_drones_data->drones[dron_index].location == LOCATION_BASE) {
        p_shm_all_drones_data->dron_in_base_count--;
    }
    if (has_space_reserved) {
        p_shm_all_drones_data->dron_reserving_space_count--;
    }
    p_shm_all_drones_data->drones[dron_index].location = LOCATION_UNDEFINE;

    if (Stack_push(free_space_stack, &dron_index) == STACK_ERROR) return -1;

    return dron_index;
};

const char *DronData_LocationToString(const DronData_Location location) {
    switch (location) {
        case LOCATION_UNDEFINE: return "LOCATION_UNDEFINE";
        case LOCATION_BASE: return "LOCATION_BASE";
        case LOCATION_LEAVING_BASE: return "LOCATION_LEAVING_BASE";
        case LOCATION_ENTERING_BASE: return "LOCATION_ENTERING_BASE";
        case LOCATION_MISSION: return "LOCATION_MISSION";
        default: return "UNKNOWN_LOCATION";
    }
}
