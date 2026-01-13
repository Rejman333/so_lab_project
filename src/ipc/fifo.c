#include "ipc.h"
#include "printer.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>



int fifo_sem_create(FIFO_SEM *fifo_sem, const char *path, const int capacity) {
    if (!fifo_sem || !path || capacity <= 0) {
        print_error("FIFO creation failed, wrong arguments");
        return -1;
    }

    memset(fifo_sem, 0, sizeof(*fifo_sem));
    fifo_sem->file_descriptor = -1;
    fifo_sem->capacity = capacity;


    const size_t path_length = strnlen(path, sizeof(fifo_sem->path));
    if (path_length >= sizeof(fifo_sem->path)) {
        print_error("Path for FIFO to long");
        return -1;
    }
    memcpy(fifo_sem->path, path, path_length + 1);

    if (mkfifo(path, 0600) == -1) {
        print_error("Failed to creat FIFO, path = '%s", path);
        return -1;
    }


    const int file_descriptor = open(path, O_RDWR);
    if (file_descriptor == -1) {
        print_error("Failed to open FIFO");
        if (unlink(fifo_sem->path) != 0) {
            print_error("Failed to unlink fifo");
            return -1;
        }
        return -1;
    }


    const int flags = fcntl(file_descriptor, F_GETFD);
    if (flags == -1) {
        close(file_descriptor);
        if (unlink(fifo_sem->path) != 0) {
            print_error("Failed to unlink fifo");
            return -1;
        }
        print_error("Failed to fcntl");
        return -1;
    }
    if (fcntl(file_descriptor, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(file_descriptor);
        if (unlink(fifo_sem->path) != 0) {
            print_error("Failed to unlink fifo");
            return -1;
        }
        print_error("Failed to fcntl");
        return -1;
    }

    const char token = 'T';
    for (int i = 0; i < capacity; ++i) {
        for (;;) {
            ssize_t w = write(file_descriptor, &token, 1);
            if (w == 1) break;
            if (w == -1 && errno == EINTR) continue;

            close(file_descriptor);
            (void) unlink(path);
            print_error("Failed at writing starter tokens to FIFO");
            return -1;
        }
    }

    fifo_sem->file_descriptor = file_descriptor;
    return 0;
}

int fifo_sem_get(FIFO_SEM *fifo_sem, const char *path) {
    if (!fifo_sem || !path) {
        print_error("FIFO creation failed, wrong arguments");
        return -1;
    }

    memset(fifo_sem, 0, sizeof(*fifo_sem));
    fifo_sem->file_descriptor = -1;
    fifo_sem->capacity = -1;

    size_t n = strnlen(path, sizeof(fifo_sem->path));
    if (n >= sizeof(fifo_sem->path)) {
        print_error("Path for FIFO to long");
        return -1;
    }
    memcpy(fifo_sem->path, path, n + 1);


    struct stat st;
    if (stat(path, &st) == -1) {
        print_error("File exist, but it is not FIFO");
        return -1;
    }

    if (!S_ISFIFO(st.st_mode)) {
        print_error("File exist, but it is not FIFO");
        return -1;
    }

    const int file_descriptor = open(path, O_RDWR);
    if (file_descriptor == -1) {
        print_error("Failed to open FIFO");
        return -1;
    }

    const int flags = fcntl(file_descriptor, F_GETFD);
    if (flags == -1) {
        close(file_descriptor);
        print_error("Failed to fcntl");
        return -1;
    }

    if (fcntl(file_descriptor, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(file_descriptor);
        print_error("Failed to fcntl");
        return -1;
    }

    fifo_sem->file_descriptor = file_descriptor;
    return 0;
}

int fifo_sem_lock(FIFO_SEM *fifo_sem) {
    if (!fifo_sem || fifo_sem->file_descriptor < 0) {
        print_error("Unable to work with incomplete fifo_sem");
        return -1;
    }

    char token;

    for (;;) {
        const ssize_t bytes_read = read(fifo_sem->file_descriptor, &token, sizeof(char));

        if (bytes_read == 1) {
            return 0;
        }

        if (bytes_read == -1 && errno == EINTR) {
            continue;
        }

        print_error("Error while reading token");
        return -1;
    }
}

int fifo_sem_unlock(FIFO_SEM *fifo_sem) {
    if (!fifo_sem || fifo_sem->file_descriptor < 0) {
        print_error("Unable to work with incomplete fifo_sem");
        return -1;
    }

    const unsigned char token = 'T';

    for (;;) {
        const ssize_t bytes_writen = write(fifo_sem->file_descriptor, &token, sizeof(char));

        if (bytes_writen == (ssize_t) sizeof(token)) {
            return 0;
        }

        if (bytes_writen == -1 && errno == EINTR) {
            continue;
        }

        print_error("Error while writing token");
        return -1;
    }
}

int fifo_sem_close(FIFO_SEM *fifo_sem) {
    if (!fifo_sem) {
        print_error("Unable to work with incomplete fifo_sem");
        return -1;
    }

    if (fifo_sem->file_descriptor >= 0) {
        for (;;) {
            const int rc = close(fifo_sem->file_descriptor);
            if (rc == 0) break;
            if (rc == -1 && errno == EINTR) continue;

            print_error("Unable to close fifo file descriptor");
            return -1;
        }
        fifo_sem->file_descriptor = -1;
    }

    return 0;
}

int fifo_sem_destroy(FIFO_SEM *fifo_sem) {
    if (!fifo_sem) {
        print_error("Unable to work with incomplete fifo_sem");
        return -1;
    }


    if (fifo_sem_close(fifo_sem) != 0) {
        print_error("Failed to close fifo file descroptor");
    }


    if (fifo_sem->path[0] == '\0') {
        print_error("No path for fifo");
        return -1;
    }
    if (unlink(fifo_sem->path) != 0) {
        print_error("Failed to unlink fifo");
        return -1;
    }
    return 0;
}

int fifo_sem_lock_timeout(FIFO_SEM *fifo_sem, const int timeout_ms) {
    if (!fifo_sem || fifo_sem->file_descriptor < 0) {
        print_error("Unable to work with incomplete fifo_sem");
        return -1;
    }

    struct pollfd pfd;
    pfd.fd = fifo_sem->file_descriptor;
    pfd.events = POLLIN;
    pfd.revents = 0;

    for (;;) {
        int prc = poll(&pfd, 1, timeout_ms);

        if (prc == 0) {
            return 1;
        }

        if (prc == -1) {
            if (errno == EINTR) continue;
            return -1;
        }

        char token;
        for (;;) {
            ssize_t r = read(fifo_sem->file_descriptor, &token, 1);
            if (r == 1) return 0;
            if (r == -1 && errno == EINTR) continue;

            print_error("Error while reading token");
            return -1;
        }
    }
}
