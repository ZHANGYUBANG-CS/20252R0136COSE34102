#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf(2, "Usage: setnice pid nice_value\n");
        exit();
    }
    int pid = atoi(argv[1]);
    int nice = atoi(argv[2]);
    int result = setnice(pid, nice);
    if (result == -1) {
        printf(2, "setnice failed\n");
    } else {
        printf(1, "Set process %d nice value to %d\n", pid, nice);
    }
    exit();
}
