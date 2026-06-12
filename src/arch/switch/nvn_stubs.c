/*
 * Stubs for NVN functions required by deko3d
 * These are proprietary Nintendo functions that are not available in devkitPro
 */

#include <switch.h>

// NVN memory mapping stubs
Result nvMapCreate(void) {
    return 0;
}

Result nvMapClose(void) {
    return 0;
}

Result nvAddressSpaceMap(void) {
    return 0;
}

Result nvAddressSpaceMapFixed(void) {
    return 0;
}

Result nvAddressSpaceUnmap(void) {
    return 0;
}

Result nvAddressSpaceModify(void) {
    return 0;
}

// ARM cache flush - use libnx implementation
void armDCacheFlush(void *addr, size_t size) {
    armDCacheFlush(addr, size);
}

// Error application stubs
Result errorApplicationCreate(void) {
    return 0;
}

void errorApplicationShow(void) {
    // Stub - do nothing
}
