#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "printer.h"
#define EINTR 4

int robie() {
    print_error("Robie nie 0");
    return -1;
}

int main() {
    setup_print("Test", COLOR_GREEN);
    if (robie() == -1) {
        print_error("Robie sie wyjebalo");
        return -1;
    }
    return 0;
}