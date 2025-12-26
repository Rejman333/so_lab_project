#include "stack.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>

struct Stack {
    size_t capacity;
    size_t size;
    size_t elem_size;
    //This corrects alignment for data[] if the data is "conventional" in alignment
    //For some crazy structs, this won`t correct alignment
    alignas(max_align_t) uint8_t data[];
};

size_t Stack_bytes_needed(const size_t capacity, const size_t elem_size) {
    return sizeof(Stack) + capacity * elem_size;
}

int Stack_init(Stack *stack, const size_t capacity, const size_t elem_size) {
    if (!stack || capacity == 0 || elem_size == 0) return STACK_ERROR;

    stack->capacity = capacity;
    stack->size = 0;
    stack->elem_size = elem_size;

    return STACK_SUCCESS;
}

int Stack_is_empty(const Stack *stack) {
    if (!stack) return STACK_ERROR;
    return stack->size == 0;
}

int Stack_is_full(const Stack *stack) {
    if (!stack) return STACK_ERROR;
    return stack->size >= stack->capacity;
}

int Stack_push(Stack *stack, const void *elem) {
    if (!stack || !elem) return STACK_ERROR;
    if (stack->size >= stack->capacity) return STACK_ERROR;

    uint8_t *dst = stack->data + (stack->size * stack->elem_size);
    memcpy(dst, elem, stack->elem_size);
    stack->size++;
    return STACK_SUCCESS;
}

int Stack_pop(Stack *stack, void *out) {
    if (!stack || !out) return STACK_ERROR;
    if (stack->size == 0) return STACK_ERROR;

    stack->size--;
    const uint8_t *src = stack->data + (stack->size * stack->elem_size);
    memcpy(out, src, stack->elem_size);
    return STACK_SUCCESS;
}

int Stack_top(const Stack *stack, void *out) {
    if (!stack || !out) return STACK_ERROR;
    if (stack->size == 0) return STACK_ERROR;

    const uint8_t *src = stack->data + ((stack->size - 1) * stack->elem_size);
    memcpy(out, src, stack->elem_size);
    return STACK_SUCCESS;
}
