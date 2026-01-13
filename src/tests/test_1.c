#include <stdio.h>
#include <stdlib.h>

#include "ipc.h"
#include "printer.h"


#define ALL_DRONES_DATA_FILE_NAME "dron_info_key_test"
#define STACK_KEY_FILE_NAME "stack_key_test"

#define PROCESS_NAME "Test_1"
#define PROCESS_COLOR COLOR_BLUE

#define MAXIMUM_DRONES_IN_MEMORY 8

int success_found = 0;
int errors_found = 0;

SHM_AllDronesData *shm_all_drones_data = NULL;
int shm_all_drones_data_id = -1;
Stack *shm_stack = NULL;
int shm_stack_id = -1;

int maximum_drones_in_memory = MAXIMUM_DRONES_IN_MEMORY;

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
        .next_dron_id = 1,
        .dron_in_base_count = 0,
        .maximum_dron_in_base_count = 0,
        .drone_lost_suicide = 0,
        .drone_lost_decommissioned = 0,
        .drone_lost_out_of_power = 0,
        .drone_lost_other = 0,
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

    return 0;
}

void print_dron_shm() {
    printf("===================\n");
    for (int i = 0; i < maximum_drones_in_memory; ++i) {
        printf("Dron: id: %d, location %d, pid: %d\n",
               shm_all_drones_data->drones[i].id,
               shm_all_drones_data->drones[i].location,
               shm_all_drones_data->drones[i].pid);
    }
    printf("===================\n");
}

void test_cleen_up() {
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

    print_msg_color(COLOR_TURQUOISE, "Final results: Successes: %d, Failures: %d", success_found, errors_found);

    exit(0);
}

int main() {
    setup_print("Test_1",COLOR_YELLOW);
    create_shm_all_drones_data();

    print_msg_color(COLOR_TURQUOISE, "=== Test Creation of SHM resources ===");

    if (shm_all_drones_data == NULL || shm_all_drones_data_id < 0) {
        print_msg_color(COLOR_RED, "FAILURE: SHM for drones not created");
        errors_found++;
    } else {
        print_msg_color(COLOR_GREEN, "Success: SHM for drones created");
        success_found++;
    }

    if (shm_stack == NULL || shm_stack_id < 0) {
        print_msg_color(COLOR_RED, "FAILURE: SHM for stack not created");
        errors_found++;
    } else {
        print_msg_color(COLOR_GREEN, "Success: SHM for stack created");
        success_found++;
    }

    if (shm_all_drones_data->capacity != MAXIMUM_DRONES_IN_MEMORY) {
        print_msg_color(
            COLOR_RED,
            "FAILURE: SHM_Drones capacity: %d, expected: %d",
            shm_all_drones_data->capacity,
            MAXIMUM_DRONES_IN_MEMORY
        );
        errors_found++;
    } else {
        print_msg_color(
            COLOR_GREEN,
            "Success: SHM_Drones capacity matches expected (%d)",
            MAXIMUM_DRONES_IN_MEMORY
        );
        success_found++;
    }

    if (Stack_get_capacity(shm_stack) != MAXIMUM_DRONES_IN_MEMORY) {
        print_msg_color(
            COLOR_RED,
            "FAILURE: SHM_Stack capacity: %d, expected: %d",
            (int) Stack_get_capacity(shm_stack),
            MAXIMUM_DRONES_IN_MEMORY
        );
        errors_found++;
    } else {
        print_msg_color(
            COLOR_GREEN,
            "Success: SHM_Stack capacity matches expected (%d)",
            MAXIMUM_DRONES_IN_MEMORY
        );
        success_found++;
    }

    if (errors_found > 0) {
        print_msg_color(COLOR_RED, "FAILURE: Creating needed resources");
        test_cleen_up();
    } else {
        print_msg_color(
            COLOR_GREEN,
            "SUCCESS: All tests passed (%d successes)",
            success_found
        );
    }

    print_msg_color(COLOR_TURQUOISE, "=== Test SHM_Drones and SHM_Stack  CRUD===");

    DronData dron_data = {.id = 1, .pid = 333, .location = LOCATION_BASE};
    int dron_index = SHM_AllDronesData_add_dron(shm_all_drones_data, shm_stack, &dron_data);
    if (dron_index == -1) {
        print_msg_color(COLOR_RED, "FAILURE: Adding dron to SHM");
        errors_found++;
        test_cleen_up();
    }

    if (shm_all_drones_data->drones[dron_index].id != dron_data.id ||
        shm_all_drones_data->drones[dron_index].location != dron_data.location) {
        print_msg_color(COLOR_RED, "FAILURE: Adding dron to SHM");
        errors_found++;
        test_cleen_up();
    }

    if (shm_all_drones_data->dron_count != 1 || shm_all_drones_data->dron_in_base_count != 1) {
        print_msg_color(COLOR_RED, "FAILURE: Adding dron to SHM, failed to set meta data");
        errors_found++;
        test_cleen_up();
    }

    print_msg_color(COLOR_GREEN, "SUCCESS: Dron added at index: %d", dron_index);
    success_found++;

    if (SHM_AllDronesData_update_dron_location(shm_all_drones_data, dron_index, LOCATION_MISSION) != 0) {
        print_msg_color(COLOR_RED, "FAILURE: Updating dron in SHM at index :%d", dron_index);
        errors_found++;
    };
    print_msg_color(COLOR_GREEN, "SUCCESS: Updating dron in SHM at index :%d", dron_index);
    success_found++;


    if (SHM_AllDronesData_delete_drone(shm_all_drones_data, shm_stack, dron_index, 0, DESTRUCTION_REASON_OTHER) != 0) {
        print_msg_color(COLOR_RED, "FAILURE: Deleting dron in SHM at index :%d", dron_index);
        errors_found++;
        test_cleen_up();
    }

    if (shm_all_drones_data->drones[dron_index].id != dron_data.id ||
        shm_all_drones_data->drones[dron_index].location != LOCATION_UNDEFINE) {
        print_msg_color(COLOR_RED, "FAILURE: Deleting dron in SHM at index :%d", dron_index);
        errors_found++;
        test_cleen_up();
    }

    if (shm_all_drones_data->dron_count != 0 || shm_all_drones_data->dron_in_base_count != 0) {
        print_msg_color(COLOR_RED, "FAILURE: Deleting dron if SHM, failed to set meta data");
        errors_found++;
        test_cleen_up();
    }

    print_msg_color(COLOR_GREEN, "SUCCESS: Deleting dron in SHM at index :%d", dron_index);
    success_found++;

    int on_top_stack = -1;
    if (Stack_top(shm_stack, &on_top_stack) != 0) {
        print_msg_color(COLOR_RED, "FAILURE: Checking top of SHM_Stack");
        errors_found++;
        test_cleen_up();
    }

    if (on_top_stack != dron_index) {
        print_msg_color(COLOR_RED, "FAILURE: Checking top of SHM_Stack, got %d, expected %d", on_top_stack, dron_index);
        errors_found++;
        test_cleen_up();
    }
    print_msg_color(COLOR_GREEN, "SUCCESS: Checking top of SHM_Stack, got %d, expected %d", on_top_stack, dron_index);
    success_found++;

    print_msg_color(COLOR_TURQUOISE, "=== Test SHM_Drones and SHM_Stack reusing space ===");

    dron_data.id = 2;
    dron_data.pid = 4444;
    dron_data.location = LOCATION_BASE;
    dron_index = -1;

    dron_index = SHM_AllDronesData_add_dron(shm_all_drones_data, shm_stack, &dron_data);
    if (dron_index == -1) {
        print_msg_color(COLOR_RED, "FAILURE: Adding dron to SHM");
        errors_found++;
        test_cleen_up();
    }

    if (shm_all_drones_data->drones[dron_index].id != dron_data.id ||
        shm_all_drones_data->drones[dron_index].location != dron_data.location) {
        print_msg_color(COLOR_RED, "FAILURE: Recalling space in SHM_Dron at index :%d", dron_index);
        errors_found++;
        test_cleen_up();
    }

    if (shm_all_drones_data->dron_count != 1 || shm_all_drones_data->dron_in_base_count != 1) {
        print_msg_color(COLOR_RED, "FAILURE: Recalling dron if SHM, failed to set meta data");
        errors_found++;
        test_cleen_up();
    }

    print_msg_color(COLOR_GREEN, "SUCCESS: Recalling space in SHM_Dron at index :%d", dron_index);
    success_found++;

    test_cleen_up();
}
