/*
 * Unix compatibility headers for Nintendo Switch
 * Maps Unix APIs to libnx equivalents or provides no-op stubs
 */

#ifndef SWITCH_COMPAT_H
#define SWITCH_COMPAT_H

#ifdef SWITCH

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

/* sys/prctl.h - no-op stubs for Switch */
#define PR_SET_NAME 15
#define PR_GET_NAME 16

static inline int prctl(int option, ...) {
    // No-op for Switch - process naming not supported
    return 0;
}

/* sys/ioctl.h - basic stub */
#define FIONREAD 0x541B

static inline int ioctl(int fd, unsigned long request, ...) {
    // Limited ioctl support for Switch
    return -1;
}

/* sys/uio.h - basic stub */
struct iovec {
    void  *iov_base;
    size_t iov_len;
};

static inline ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return -1;
}

static inline ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return -1;
}

/* linux/capability.h - no-op stubs for Switch */
#define CAP_SYS_ADMIN 21
#define CAP_SYS_NICE 23
#define _LINUX_CAPABILITY_VERSION_3 0x20080522

struct __user_cap_header_struct {
    uint32_t version;
    int pid;
};

struct __user_cap_data_struct {
    uint32_t effective;
    uint32_t permitted;
    uint32_t inheritable;
};

static inline int capget(void *hdrp, void *datap) {
    return -1;
}

static inline int capset(void *hdrp, void *datap) {
    return -1;
}

/* sys/syscall.h - no-op stubs for Switch */
#define SYS_capget 184
#define SYS_syncfs 306

static inline long syscall(long number, ...) {
    return -1;
}

/* sched.h - CPU affinity stubs for Switch */
#define CPU_SETSIZE 1024
typedef struct {
    unsigned long __bits[CPU_SETSIZE / (8 * sizeof(unsigned long))];
} cpu_set_t;

static inline void CPU_ZERO(cpu_set_t *set) {
    for (int i = 0; i < CPU_SETSIZE / (8 * sizeof(unsigned long)); i++)
        set->__bits[i] = 0;
}

static inline void CPU_SET(int cpu, cpu_set_t *set) {
    set->__bits[cpu / (8 * sizeof(unsigned long))] |= (1UL << (cpu % (8 * sizeof(unsigned long))));
}

static inline int CPU_ISSET(int cpu, cpu_set_t *set) {
    return (set->__bits[cpu / (8 * sizeof(unsigned long))] & (1UL << (cpu % (8 * sizeof(unsigned long))))) != 0;
}

static inline int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
    // Return all CPUs available for Switch
    CPU_ZERO(mask);
    for (int i = 0; i < 4; i++) // Switch has 4 cores
        CPU_SET(i, mask);
    return 0;
}

/* syslog.h - no-op stubs for Switch */
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_USER 1
#define LOG_LOCAL0 16
#define LOG_LOCAL1 17
#define LOG_LOCAL2 18
#define LOG_LOCAL3 19
#define LOG_LOCAL4 20
#define LOG_LOCAL5 21
#define LOG_LOCAL6 22
#define LOG_LOCAL7 23

static inline void openlog(const char *ident, int option, int facility) {
    // No-op for Switch
}

static inline void syslog(int priority, const char *format, ...) {
    // No-op for Switch
}

static inline void closelog(void) {
    // No-op for Switch
}

/* sys/utsname.h - stubs for Switch */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

static inline int uname(struct utsname *buf) {
    // Return Switch-specific info
    strcpy(buf->sysname, "HorizonOS");
    strcpy(buf->nodename, "Nintendo Switch");
    strcpy(buf->release, "1.0.0");
    strcpy(buf->version, "Nintendo Switch");
    strcpy(buf->machine, "aarch64");
    strcpy(buf->domainname, "(none)");
    return 0;
}

/* sys/mman.h - stubs for Switch */
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    // Use malloc for Switch - not ideal but functional
    return malloc(length);
}

static inline int munmap(void *addr, size_t length) {
    free(addr);
    return 0;
}

/* stdio.h - off64_t typedef for Switch */
typedef off_t off64_t;

/* time.h - timegm stub for Switch (not available in libnx) */
static inline time_t timegm(struct tm *tm) {
    // Convert to UTC time - simplified version for Switch
    // This is a rough approximation
    return mktime(tm);
}

/* sys/ioctl.h - no-op stubs for Switch */
#define ioctl(fd, request, ...) (-1)

/* linux/input.h - minimal stubs for Switch */
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define EV_SYN 0x00

#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

#define REL_X 0x00
#define REL_Y 0x01

#define ABS_X 0x00
#define ABS_Y 0x01

/* poll.h - minimal stubs for Switch */
#define POLLIN 0x001
#define POLLOUT 0x004
#define POLLERR 0x008

typedef unsigned long nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

static inline int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    // Not available on Switch - return error
    return -1;
}

/* sys/socket.h - minimal stubs for Switch */
typedef unsigned int socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

#define AF_INET 2
#define AF_INET6 10
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_NONBLOCK 04000
#define SOCK_CLOEXEC 02000000

#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_ERROR 4

static inline int socket(int domain, int type, int protocol) {
    return -1;
}

static inline int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return -1;
}

static inline int listen(int sockfd, int backlog) {
    return -1;
}

static inline int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return -1;
}

static inline int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return -1;
}

static inline int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return -1;
}

static inline int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return -1;
}

/* netinet/in.h - minimal stubs for Switch */
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IP 0
#define IP_ADD_MEMBERSHIP 35

typedef uint16_t in_port_t;
typedef uint16_t sa_family_t;
typedef uint32_t in_addr_t;

struct in_addr {
    uint32_t s_addr;
};

struct in6_addr {
    uint8_t s6_addr[16];
};

struct ip_mreq {
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char __ss_pad1[6];
    uint64_t __ss_align;
    char __ss_pad2[112];
};

/* netdb.h - stubs for Switch */
#define HOST_NOT_FOUND 1
#define NO_ADDRESS 4
#define NO_RECOVERY 5
#define TRY_AGAIN 2
#define INADDR_NONE ((in_addr_t)-1)

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

static inline struct hostent *gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop) {
    return NULL;
}

static inline in_addr_t inet_addr(const char *cp) {
    return INADDR_NONE;
}

static inline int inet_pton(int af, const char *src, void *dst) {
    return -1;
}

/* arpa/inet.h - byte swapping stubs */
static inline uint16_t htons(uint16_t hostshort) {
    return hostshort;
}

static inline uint16_t ntohs(uint16_t netshort) {
    return netshort;
}

/* sys/socket.h - additional stubs */
#define SO_KEEPALIVE 9
#define SO_BROADCAST 6
#define SO_RCVBUF 8
#define SO_RCVTIMEO 20
#define TCP_NODELAY 7
#define MSG_WAITALL 0x100
#define SHUT_RDWR 2

static inline int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return -1;
}

static inline int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return -1;
}

static inline ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return -1;
}

static inline ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return -1;
}

static inline ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return -1;
}

static inline ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    return -1;
}

static inline int shutdown(int sockfd, int how) {
    return -1;
}

/* poll.h - additional constants */
#define POLLHUP 0x010

/* ifaddrs.h - stubs for Switch */
#define IFF_UP 0x1
#define IFF_LOOPBACK 0x8
#define IFF_RUNNING 0x40

struct ifaddrs {
    struct ifaddrs *ifa_next;
    char *ifa_name;
    unsigned int ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    struct sockaddr *ifa_dstaddr;
};

static inline int getifaddrs(struct ifaddrs **ifap) {
    return -1;
}

static inline void freeifaddrs(struct ifaddrs *ifa) {
    // No-op
}

#endif /* SWITCH */

#endif /* SWITCH_COMPAT_H */
