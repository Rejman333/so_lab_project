#include <stdio.h>
#include <sys/resource.h>

static void print_rlimit(int res, const char *name) {
    struct rlimit r;
    if (getrlimit(res, &r) != 0) { perror("getrlimit"); return; }

    unsigned long long cur = (r.rlim_cur == RLIM_INFINITY) ? 0 : (unsigned long long)r.rlim_cur;
    unsigned long long max = (r.rlim_max == RLIM_INFINITY) ? 0 : (unsigned long long)r.rlim_max;

    printf("%s: cur=%s%llu  max=%s%llu\n",
           name,
           (r.rlim_cur == RLIM_INFINITY) ? "INF(" : "",
           (r.rlim_cur == RLIM_INFINITY) ? 0ULL : cur,
           (r.rlim_cur == RLIM_INFINITY) ? ")" : "",
           (r.rlim_max == RLIM_INFINITY) ? "INF(" : "",
           (r.rlim_max == RLIM_INFINITY) ? 0ULL : max,
           (r.rlim_max == RLIM_INFINITY) ? ")" : "");
}

int main(void) {
    print_rlimit(RLIMIT_NPROC,  "RLIMIT_NPROC (proc/threads per user)");
    print_rlimit(RLIMIT_NOFILE, "RLIMIT_NOFILE (open files)");
    print_rlimit(RLIMIT_STACK,  "RLIMIT_STACK (stack bytes)");
    print_rlimit(RLIMIT_AS,     "RLIMIT_AS (virtual memory bytes)");
    return 0;
}