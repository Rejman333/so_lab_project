#include <fcntl.h>
#include <unistd.h>
#include <bits/fcntl-linux.h>
#include <sys/ipc.h>

#include "ipc.h"

key_t grab_key_from_file(const char *file_name) {
    int fd = open(file_name, O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        print_error("Failed to open or create key file: %s", file_name);
        return -1;
    }

    if (close(fd) == -1) {
        print_error("Failed to close key file: %s", file_name);
        return -1;
    }

    key_t key = ftok(file_name, 1);
    if (key == -1) {
        print_error("Failed to generate key using ftok() for file: %s", file_name);
        return -1;
    }

    return key;
}
