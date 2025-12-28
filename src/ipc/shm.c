#include <string.h>

#include "ipc.h"

#include <unistd.h>
#include <sys/shm.h>

#include "printer.h"

int shm_create(key_t key, size_t size) {
    int shmid = shmget(key, size, 0600 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1) {
        print_error("Failed to create sheared memory");
        _exit(1);
    }
    return shmid;
};

int shm_open_existing(key_t key) {
    int shmid = shmget(key, 0, 0600);
    if (shmid == -1) {
        print_error("shm_open_existing: shmget");
        return -1;
    }
    return shmid;
}

void *shm_attach(int shmid) {
    void *addr = shmat(shmid, NULL, 0);

    if (addr == (void *) -1) {
        print_error("shm_attach: shmat");
        return NULL;
    }

    return addr;
}

int shm_detach(const void *addr) {
    if (shmdt(addr) == -1) {
        print_error("shm_detach: shmdt");
        return -1;
    }
    return 0;
}

int shm_destroy(int shmid) {
    // Mark for deletion
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        print_error("shm_destroy: shmctl(IPC_RMID)");
        return -1;
    }
    return 0;
}

int SHM_DronInfo_add_dron(SHM_DronInfo *p_shm_dron_info, Stack *free_space_stack, Dron_State *p_dron_state) {
    int out = -1;
    if (Stack_pop(free_space_stack, &out) == STACK_ERROR) return -1;

    p_shm_dron_info->dron_count++;
    if (p_dron_state->dron_location == LOCATION_BASE) p_shm_dron_info->dron_in_base_count++;
    memcpy(&p_shm_dron_info->dron_state_array[out], p_dron_state, sizeof(Dron_State));

    return out;
};

int SHM_DronInfo_mission_completed(SHM_DronInfo *p_shm_dron_info) {
    p_shm_dron_info->missions_completed_count++;
    return p_shm_dron_info->missions_completed_count;
};

int SHM_DronInfo_update_dron_location(SHM_DronInfo *p_shm_dron_info, int dron_index, Location new_dron_location) {
    Location old_location = p_shm_dron_info->dron_state_array[dron_index].dron_location;
    if (old_location == LOCATION_BASE) p_shm_dron_info->dron_in_base_count--;
    p_shm_dron_info->dron_state_array[dron_index].dron_location = new_dron_location;
    return 0;
};

int SHM_DronInfo_delete_drone(SHM_DronInfo *p_shm_dron_info, Stack *free_space_stack, int dron_index) {
    p_shm_dron_info->dron_count--;
    p_shm_dron_info->drone_lost_count++;
    if (p_shm_dron_info->dron_state_array[dron_index].dron_location == LOCATION_BASE) {
        p_shm_dron_info->dron_in_base_count--;
    }
    p_shm_dron_info->dron_state_array[dron_index].dron_location = LOCATION_UNDEFINE;

    if (Stack_push(free_space_stack, &dron_index) == STACK_ERROR) return -1;

    return dron_index;
};
