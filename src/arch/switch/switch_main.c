/*
 * Movian for Nintendo Switch
 * Main entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <switch.h>
#include <switch/services/bsd.h>
#include "compiler.h"
#include "arch/arch.h"

extern int showtime_main(int argc, char **argv);
extern void sqlite3_switch_vfs_register(void);

void attribute_noreturn arch_exit(void) {
    bsdExit();
    exit(0);
}

int arch_stop_req(void) {
    return 0;
}

int64_t arch_get_avtime(void) {
    uint64_t ticks = svcGetSystemTick();
    // Convert ticks to microseconds (Switch runs at 192MHz)
    return (ticks * 1000000) / 192000000;
}

int main(int argc, char **argv) {
    // Initialize libnx networking (BSD sockets)
    Result rc = bsdInitialize(NULL, 0x0, 0x2000);
    if (R_FAILED(rc)) {
        printf("Failed to initialize bsd: 0x%x\n", rc);
        return 1;
    }

    // Initialize SQLite VFS for Switch
    sqlite3_switch_vfs_register();

    // Run Movian
    int ret = showtime_main(argc, argv);

    // Cleanup
    bsdExit();
    return ret;
}
