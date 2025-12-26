#pragma once

#include <stddef.h>

/*
 * This implementation of stack is created with sheared memory in mind.
 * It`s not the best way of doing things,
 * if your problem is more generic.
 */

#define STACK_SUCCESS 1
#define STACK_ERROR -1

typedef struct Stack Stack;

size_t Stack_bytes_needed(const size_t capacity, const size_t elem_size);

int Stack_init(Stack *stack, size_t capacity, size_t elem_size);

int Stack_is_empty(const Stack *stack);

int Stack_is_full(const Stack *stack);

int Stack_push(Stack *stack, const void *elem);

int Stack_pop(Stack *stack, void *out);

int Stack_top(const Stack *stack, void *out);
