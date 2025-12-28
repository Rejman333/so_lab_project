#include "data_structures/stack.h"
#include "ipc/ipc.h"
#include "printer.h"

#include <stdlib.h>
#include <unistd.h>


int main() {
    setup_print("test", COLOR_GREEN);
    print_msg("Hello!");

    int max_elem = 10;


    size_t bytes_needed = Stack_bytes_needed(max_elem, sizeof(int));
    print_msg("We need bytes %zu", bytes_needed);


    int shm_stack_id = shm_create(3333, bytes_needed);
    Stack *stack = shm_attach(shm_stack_id);
    Stack_init(stack, bytes_needed, bytes_needed);

    Stack_init(stack, max_elem, sizeof(int));
    if (!stack) {
        print_error("Malloc nei zadziaałał");
        return -1;
    }

    for (int i = max_elem - 1; i >= 0; --i) {
        Stack_push(stack, &i);
    }

    int is_empty = Stack_is_empty(stack);
    int is_full = Stack_is_full(stack);
    print_msg("Stack is_empty %d, stack is full %d", is_empty, is_full);

    for (int i = 0; i < max_elem + 2; ++i) {
        int out = -1;
        if (Stack_pop(stack, &out)) print_error("Error in stack");
        print_msg("Element: %d", out);
    }

    is_empty = Stack_is_empty(stack);
    is_full = Stack_is_full(stack);
    print_msg("Stack is_empty %d, stack is full %d", is_empty, is_full);

    shm_detach(stack);
    shm_destroy(shm_stack_id);
    return 0;
}
