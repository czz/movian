/*
 * SQLite VFS (Virtual File System) for Nintendo Switch
 * Uses POSIX file I/O which is available via libnx/newlib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include "sqlite3.h"

/* Switch VFS name */
#define SWITCH_VFS_NAME "switch"

/* File handle structure */
typedef struct {
    sqlite3_file base;
    int fd;
    char *zPath;
} SwitchFile;

/* VFS structure */
typedef struct {
    sqlite3_vfs base;
} SwitchVFS;

/*
 * Close a file
 */
static int switchClose(sqlite3_file *pFile) {
    SwitchFile *p = (SwitchFile *)pFile;
    if (p->fd >= 0) {
        close(p->fd);
        p->fd = -1;
    }
    if (p->zPath) {
        free(p->zPath);
        p->zPath = NULL;
    }
    return SQLITE_OK;
}

/*
 * Read from file
 */
static int switchRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst) {
    SwitchFile *p = (SwitchFile *)pFile;
    ssize_t nRead;
    off_t origPos;
    
    if (p->fd < 0) return SQLITE_IOERR;
    
    /* Save current position */
    origPos = lseek(p->fd, 0, SEEK_CUR);
    if (origPos < 0) return SQLITE_IOERR_READ;
    
    /* Seek to offset */
    if (lseek(p->fd, (off_t)iOfst, SEEK_SET) < 0) {
        return SQLITE_IOERR_READ;
    }
    
    /* Read data */
    nRead = read(p->fd, zBuf, iAmt);
    
    /* Restore position */
    lseek(p->fd, origPos, SEEK_SET);
    
    if (nRead < 0) {
        return SQLITE_IOERR_READ;
    }
    if (nRead < iAmt) {
        /* Short read - fill remainder with zeros */
        memset((char *)zBuf + nRead, 0, iAmt - nRead);
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

/*
 * Write to file
 */
static int switchWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst) {
    SwitchFile *p = (SwitchFile *)pFile;
    ssize_t nWrite;
    off_t origPos;
    
    if (p->fd < 0) return SQLITE_IOERR;
    
    /* Save current position */
    origPos = lseek(p->fd, 0, SEEK_CUR);
    if (origPos < 0) return SQLITE_IOERR_WRITE;
    
    /* Seek to offset */
    if (lseek(p->fd, (off_t)iOfst, SEEK_SET) < 0) {
        return SQLITE_IOERR_WRITE;
    }
    
    /* Write data */
    nWrite = write(p->fd, zBuf, iAmt);
    
    /* Restore position */
    lseek(p->fd, origPos, SEEK_SET);
    
    if (nWrite < 0) {
        return SQLITE_IOERR_WRITE;
    }
    if (nWrite < iAmt) {
        return SQLITE_IOERR_WRITE;
    }
    return SQLITE_OK;
}

/*
 * Truncate file
 */
static int switchTruncate(sqlite3_file *pFile, sqlite3_int64 size) {
    SwitchFile *p = (SwitchFile *)pFile;
    
    if (p->fd < 0) return SQLITE_IOERR;
    
    if (ftruncate(p->fd, (off_t)size) != 0) {
        return SQLITE_IOERR_TRUNCATE;
    }
    return SQLITE_OK;
}

/*
 * Sync file to disk
 */
static int switchSync(sqlite3_file *pFile, int flags) {
    SwitchFile *p = (SwitchFile *)pFile;
    
    if (p->fd < 0) return SQLITE_IOERR;
    
    if (fsync(p->fd) != 0) {
        return SQLITE_IOERR_FSYNC;
    }
    return SQLITE_OK;
}

/*
 * Get file size
 */
static int switchFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize) {
    SwitchFile *p = (SwitchFile *)pFile;
    struct stat buf;
    
    if (p->fd < 0) return SQLITE_IOERR;
    
    if (fstat(p->fd, &buf) != 0) {
        return SQLITE_IOERR_FSTAT;
    }
    *pSize = buf.st_size;
    return SQLITE_OK;
}

/*
 * Lock file (no-op for single-threaded)
 */
static int switchLock(sqlite3_file *pFile, int eLock) {
    return SQLITE_OK;
}

/*
 * Unlock file (no-op for single-threaded)
 */
static int switchUnlock(sqlite3_file *pFile, int eLock) {
    return SQLITE_OK;
}

/*
 * Check reserved lock (no-op for single-threaded)
 */
static int switchCheckReservedLock(sqlite3_file *pFile, int *pResOut) {
    *pResOut = 0;
    return SQLITE_OK;
}

/*
 * File control
 */
static int switchFileControl(sqlite3_file *pFile, int op, void *pArg) {
    return SQLITE_NOTFOUND;
}

/*
 * Sector size
 */
static int switchSectorSize(sqlite3_file *pFile) {
    return 4096;
}

/*
 * Device characteristics
 */
static int switchDeviceCharacteristics(sqlite3_file *pFile) {
    return SQLITE_IOCAP_ATOMIC4K | SQLITE_IOCAP_SAFE_APPEND;
}

/* File methods */
static const sqlite3_io_methods switchIoMethods = {
    1,                              /* iVersion */
    switchClose,                    /* xClose */
    switchRead,                     /* xRead */
    switchWrite,                    /* xWrite */
    switchTruncate,                 /* xTruncate */
    switchSync,                     /* xSync */
    switchFileSize,                 /* xFileSize */
    switchLock,                     /* xLock */
    switchUnlock,                   /* xUnlock */
    switchCheckReservedLock,         /* xCheckReservedLock */
    switchFileControl,              /* xFileControl */
    switchSectorSize,               /* xSectorSize */
    switchDeviceCharacteristics     /* xDeviceCharacteristics */
};

/*
 * Open a file
 */
static int switchOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags) {
    SwitchFile *p = (SwitchFile *)pFile;
    int oflags = 0;
    int fd;
    
    memset(p, 0, sizeof(SwitchFile));
    p->fd = -1;
    p->zPath = zName ? strdup(zName) : NULL;
    
    /* Convert SQLite flags to POSIX flags */
    if (flags & SQLITE_OPEN_READONLY) {
        oflags |= O_RDONLY;
    }
    if (flags & SQLITE_OPEN_READWRITE) {
        oflags |= O_RDWR;
    }
    if (flags & SQLITE_OPEN_CREATE) {
        oflags |= O_CREAT;
    }
    if (flags & SQLITE_OPEN_EXCLUSIVE) {
        oflags |= O_EXCL;
    }
    
    /* Open the file */
    fd = open(zName, oflags, 0644);
    if (fd < 0) {
        if (p->zPath) free(p->zPath);
        return SQLITE_CANTOPEN;
    }
    
    p->fd = fd;
    p->base.pMethods = &switchIoMethods;
    
    if (pOutFlags) {
        *pOutFlags = flags;
    }
    
    return SQLITE_OK;
}

/*
 * Delete a file
 */
static int switchDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir) {
    if (unlink(zName) != 0) {
        return SQLITE_IOERR_DELETE;
    }
    return SQLITE_OK;
}

/*
 * Check file access
 */
static int switchAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut) {
    int rc;
    
    if (flags == SQLITE_ACCESS_EXISTS) {
        struct stat buf;
        rc = stat(zName, &buf);
        *pResOut = (rc == 0) ? 1 : 0;
    } else if (flags == SQLITE_ACCESS_READ) {
        rc = access(zName, R_OK);
        *pResOut = (rc == 0) ? 1 : 0;
    } else if (flags == SQLITE_ACCESS_READWRITE) {
        rc = access(zName, R_OK | W_OK);
        *pResOut = (rc == 0) ? 1 : 0;
    } else {
        *pResOut = 0;
    }
    
    return SQLITE_OK;
}

/*
 * Get full pathname
 */
static int switchFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut) {
    snprintf(zOut, nOut, "%s", zName);
    return SQLITE_OK;
}

/*
 * No-op for dynamic loading
 */
static void *switchDlOpen(sqlite3_vfs *pVfs, const char *zName) {
    return NULL;
}

static void switchDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg) {
    snprintf(zErrMsg, nByte, "dynamic loading not supported");
}

static void (*switchDlSym(sqlite3_vfs *pVfs, void *p, const char *zSym))(void) {
    return NULL;
}

static void switchDlClose(sqlite3_vfs *pVfs, void *p) {
}

/*
 * Randomness
 */
static int switchRandomness(sqlite3_vfs *pVfs, int nByte, char *zBuf) {
    /* Use /dev/urandom if available, otherwise use a simple fallback */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, zBuf, nByte);
        close(fd);
    } else {
        /* Simple fallback - not cryptographically secure */
        int i;
        for (i = 0; i < nByte; i++) {
            zBuf[i] = rand() & 0xFF;
        }
    }
    return nByte;
}

/*
 * Sleep
 */
static int switchSleep(sqlite3_vfs *pVfs, int microseconds) {
    usleep(microseconds);
    return microseconds;
}

/*
 * Current time
 */
static int switchCurrentTime(sqlite3_vfs *pVfs, double *pTime) {
    /* Use clock_gettime instead of gettimeofday */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    *pTime = ts.tv_sec + ts.tv_nsec / 1000000000.0;
    return SQLITE_OK;
}

/*
 * Get last error
 */
static int switchGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf) {
    if (nBuf > 0) {
        snprintf(zBuf, nBuf, "%s", strerror(errno));
    }
    return 0;
}

/*
 * Initialize the Switch VFS
 */
int sqlite3_switch_vfs_init(void) {
    static SwitchVFS switchVfs;
    static sqlite3_vfs *pDefaultVfs;
    
    memset(&switchVfs, 0, sizeof(switchVfs));
    
    /* Get default VFS to check if we need to replace it */
    pDefaultVfs = sqlite3_vfs_find(NULL);
    
    switchVfs.base.iVersion = 1;
    switchVfs.base.szOsFile = sizeof(SwitchFile);
    switchVfs.base.mxPathname = 512;
    switchVfs.base.pNext = pDefaultVfs;
    switchVfs.base.zName = SWITCH_VFS_NAME;
    switchVfs.base.pAppData = NULL;
    switchVfs.base.xOpen = switchOpen;
    switchVfs.base.xDelete = switchDelete;
    switchVfs.base.xAccess = switchAccess;
    switchVfs.base.xFullPathname = switchFullPathname;
    switchVfs.base.xDlOpen = switchDlOpen;
    switchVfs.base.xDlError = switchDlError;
    switchVfs.base.xDlSym = switchDlSym;
    switchVfs.base.xDlClose = switchDlClose;
    switchVfs.base.xRandomness = switchRandomness;
    switchVfs.base.xSleep = switchSleep;
    switchVfs.base.xCurrentTime = switchCurrentTime;
    switchVfs.base.xGetLastError = switchGetLastError;
    
    return sqlite3_vfs_register(&switchVfs.base, 1);
}
