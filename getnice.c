#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf(2, "Usage: getnice pid\n");
        exit();
    }
    int pid = atoi(argv[1]);
    int nice = getnice(pid);
    if (nice == -1) {
        printf(2, "getnice failed\n");
    } else {
        printf(1, "Process %d nice value: %d\n", pid, nice);
    }
    exit();
}
