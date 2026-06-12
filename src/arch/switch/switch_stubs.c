/*
 * Movian for Nintendo Switch
 * Stub implementations for missing functions
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <switch.h>
#include <errno.h>
#include <stdio.h>
#include <regex.h>
#include <libgen.h>
#include <ctype.h>
#include "networking/net.h"
#include "arch/halloc.h"

/* NetBIOS resolution - simplified implementation */
int nmb_resolve(const char *hostname, struct net_addr *na) {
    // NetBIOS resolution requires NBNS protocol implementation
    // This is complex and would require external library
    // For Switch, we provide a stub that returns error
    // SMB can still work with direct IP addresses
    
    // If hostname is already an IP address, parse it
    if (hostname && (hostname[0] >= '0' && hostname[0] <= '9')) {
        // Try to parse as IPv4 address
        unsigned char ip[4];
        int parts = sscanf(hostname, "%hhu.%hhu.%hhu.%hhu", 
                         &ip[0], &ip[1], &ip[2], &ip[3]);
        if (parts == 4) {
            na->na_family = 2; // AF_INET
            na->na_port = 0;
            memcpy(&na->na_addr, ip, 4);
            return 0;
        }
    }
    
    // NetBIOS name resolution not implemented
    // Users should use IP addresses for SMB shares
    return -1;
}

/* showtime_main - real implementation for Switch */
extern void main_init(void);
extern void main_fini(void);
extern void parse_opts(int argc, char **argv);
extern int arch_stop_req(void);

int showtime_main(int argc, char **argv) {
    // Parse command line options
    parse_opts(argc, argv);

    // Initialize Movian core
    main_init();

    // Start GLW UI for Switch
    extern int glw_switch_main(void);
    glw_switch_main();

    // Cleanup
    main_fini();

    return 0;
}

/* arch stubs - implemented with libnx APIs */
int64_t arch_get_ts(void) { 
    return armGetSystemTick(); 
}

void arch_get_random_bytes(void *buf, size_t len) {
    randomGet(buf, len);
}

struct tm *arch_localtime(const time_t *timep) { 
    return localtime(timep); 
}

void trace_arch(void) {
    // Stub - could implement console logging if needed
}

/* memory allocation stubs - needed by fileaccess, pool, websocket */
void hfree(void *ptr, size_t size) {
    free(ptr);
}
void *halloc(size_t size) { return malloc(size); }
void *mymalloc(size_t size) { return malloc(size); }
void *myrealloc(void *ptr, size_t size) { return realloc(ptr, size); }
void *mymemalign(size_t alignment, size_t size) {
    return memalign(alignment, size);
}

/* POSIX pipe() - simple implementation using event handles */
static int pipe_counter = 0;
static int pipe_fds[100][2] = {0};

int pipe(int pipefd[2]) {
    // Simple pipe implementation using counter-based FDs
    // This is a simplified version for Switch
    if (pipe_counter >= 100) {
        return -1; // Too many pipes
    }
    pipefd[0] = pipe_counter * 2;
    pipefd[1] = pipe_counter * 2 + 1;
    pipe_fds[pipe_counter][0] = 1;
    pipe_fds[pipe_counter][1] = 1;
    pipe_counter++;
    return 0;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    // Use memalign instead
    *memptr = memalign(alignment, size);
    return (*memptr == NULL) ? ENOMEM : 0;
}

/* POSIX regex - extended implementation with character classes */
static char *regex_patterns[100] = {NULL};
static int regex_flags[100] = {0};
static int regex_pattern_count = 0;

static int match_char_class(const char *pattern, char c) {
    // Match character class [a-z], [0-9], etc.
    int invert = 0;
    const char *p = pattern + 1; // Skip '['
    
    if (*p == '^') {
        invert = 1;
        p++;
    }
    
    int matched = 0;
    while (*p && *p != ']') {
        if (*(p + 1) == '-' && *(p + 2) && *(p + 2) != ']') {
            // Range like a-z
            if (c >= *p && c <= *(p + 2)) {
                matched = 1;
                break;
            }
            p += 3;
        } else {
            // Single character
            if (c == *p) {
                matched = 1;
                break;
            }
            p++;
        }
    }
    
    return invert ? !matched : matched;
}

static int parse_char_class_len(const char *pattern) {
    // Return length of character class pattern
    const char *p = pattern + 1; // Skip '['
    while (*p && *p != ']') p++;
    if (*p == ']') return p - pattern + 1;
    return 1; // Invalid, just match '['
}

static int advanced_match(const char *pattern, const char *string, int flags) {
    // Extended regex matching with character classes
    if (*pattern == '\0' && *string == '\0') return 1;
    if (*pattern == '\0') return 0;
    
    if (*pattern == '*') {
        if (*(pattern + 1) == '\0') return 1;
        if (advanced_match(pattern + 1, string, flags)) return 1;
        if (*string != '\0' && advanced_match(pattern, string + 1, flags)) return 1;
        return 0;
    }
    
    if (*pattern == '?') {
        if (*string == '\0') return 0;
        return advanced_match(pattern + 1, string + 1, flags);
    }
    
    if (*pattern == '[') {
        if (*string == '\0') return 0;
        int len = parse_char_class_len(pattern);
        if (match_char_class(pattern, *string)) {
            return advanced_match(pattern + len, string + 1, flags);
        }
        return 0;
    }
    
    if (*pattern == '.') {
        if (*string == '\0') return 0;
        return advanced_match(pattern + 1, string + 1, flags);
    }
    
    if (*pattern == '\\') {
        // Escaped character
        if (*(pattern + 1) == '\0') return 0;
        if (*string == *(pattern + 1)) {
            return advanced_match(pattern + 2, string + 1, flags);
        }
        return 0;
    }
    
    // Case insensitive matching if REG_ICASE flag is set
    char p = *pattern;
    char s = *string;
    if (flags & REG_ICASE) {
        p = tolower(p);
        s = tolower(s);
    }
    
    if (p == s) {
        return advanced_match(pattern + 1, string + 1, flags);
    }
    
    return 0;
}

int regcomp(regex_t *preg, const char *regex, int cflags) {
    // Store the regex pattern in a static array
    if (regex_pattern_count >= 100) {
        return REG_ESPACE;
    }
    regex_patterns[regex_pattern_count] = strdup(regex);
    if (regex_patterns[regex_pattern_count] == NULL) {
        return REG_ESPACE;
    }
    regex_flags[regex_pattern_count] = cflags;
    preg->re_nsub = regex_pattern_count;
    regex_pattern_count++;
    return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    int idx = preg->re_nsub;
    if (idx < 0 || idx >= regex_pattern_count || regex_patterns[idx] == NULL) {
        return REG_INVARG;
    }
    
    const char *pattern = regex_patterns[idx];
    int flags = regex_flags[idx];
    int match = advanced_match(pattern, string, flags);
    
    if (match && nmatch > 0 && !(flags & REG_NOSUB)) {
        pmatch[0].rm_so = 0;
        pmatch[0].rm_eo = strlen(string);
    }
    
    return match ? 0 : REG_NOMATCH;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    const char *msg;
    switch (errcode) {
        case REG_NOMATCH: msg = "No match"; break;
        case REG_INVARG: msg = "Invalid argument"; break;
        case REG_ESPACE: msg = "Out of memory"; break;
        default: msg = "Unknown error"; break;
    }
    size_t len = strlen(msg) + 1;
    if (errbuf_size > 0) {
        strncpy(errbuf, msg, errbuf_size - 1);
        errbuf[errbuf_size - 1] = '\0';
    }
    return len;
}

void regfree(regex_t *preg) {
    int idx = preg->re_nsub;
    if (idx >= 0 && idx < regex_pattern_count && regex_patterns[idx] != NULL) {
        free(regex_patterns[idx]);
        regex_patterns[idx] = NULL;
    }
}

/* POSIX popen/pclose - simulated with file search for Switch */
#include <dirent.h>
#include <ctype.h>

static FILE *popen_streams[10] = {NULL};
static char popen_commands[10][256] = {0};
static int popen_count = 0;

static char *extract_query_from_locate(const char *cmd) {
    // Extract query from "locate -i -L -q -b 'query'"
    const char *start = strchr(cmd, '\'');
    if (!start) return NULL;
    start++;
    const char *end = strchr(start, '\'');
    if (!end) return NULL;
    
    size_t len = end - start;
    char *query = malloc(len + 1);
    if (query) {
        memcpy(query, start, len);
        query[len] = '\0';
    }
    return query;
}

static int matches_pattern(const char *name, const char *pattern, int case_insensitive) {
    // Simple pattern matching for file search
    if (!pattern || !*pattern) return 1;
    if (!name) return 0;
    
    while (*pattern && *name) {
        if (case_insensitive) {
            if (tolower(*pattern) != tolower(*name)) return 0;
        } else {
            if (*pattern != *name) return 0;
        }
        pattern++;
        name++;
    }
    return (*pattern == '\0' && *name == '\0');
}

FILE *popen(const char *command, const char *type) {
    // Simulated popen for locate command - performs direct file search
    // Process spawning not available on Switch
    
    char *query = extract_query_from_locate(command);
    if (!query) return NULL;
    
    // Create temporary file to store results
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/locate_%d.txt", popen_count);
    
    FILE *fp = fopen(tmp_path, "w");
    if (!fp) {
        free(query);
        return NULL;
    }
    
    // Search in common directories
    const char *search_dirs[] = {"/", "/sdmc", "/switch", "/config", NULL};
    
    for (int d = 0; search_dirs[d]; d++) {
        DIR *dir = opendir(search_dirs[d]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (matches_pattern(entry->d_name, query, 1)) {
                fprintf(fp, "%s/%s\n", search_dirs[d], entry->d_name);
            }
        }
        closedir(dir);
    }
    
    fclose(fp);
    free(query);
    
    // Reopen for reading
    fp = fopen(tmp_path, type);
    if (fp) {
        if (popen_count < 10) {
            popen_streams[popen_count] = fp;
            strncpy(popen_commands[popen_count], tmp_path, sizeof(popen_commands[popen_count]) - 1);
            popen_count++;
        }
    }
    
    return fp;
}

int pclose(FILE *stream) {
    // Close stream and cleanup temp file
    for (int i = 0; i < 10; i++) {
        if (popen_streams[i] == stream) {
            fclose(stream);
            remove(popen_commands[i]);
            popen_streams[i] = NULL;
            popen_commands[i][0] = '\0';
            return 0;
        }
    }
    return -1;
}

/* POSIX basename stub - needed by fa_locatedb */
char *basename(char *path) {
    // Simple implementation of basename
    if (!path || !*path) {
        return ".";
    }
    
    char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }
    return path;
}

/* SQLite OS-specific stubs for Switch */
const char sqlite3_version[] = "3.45.0";

int sqlite3_unlock_notify(sqlite3 *db, void (*cb)(void **, int), void *p) {
    // Not available with custom VFS
    return SQLITE_ERROR;
}

/* VFS initialization function */
int sqlite3_switch_vfs_init(void);

/* Initialize Switch VFS */
void sqlite3_switch_vfs_register(void) {
    sqlite3_switch_vfs_init();
}

/* POSIX stubs for SQLite Unix VFS */
long sysconf(int name) {
    switch (name) {
        case 30: /* _SC_PAGESIZE */
            return 4096;
        default:
            return -1;
    }
}

int fchown(int fd, uid_t owner, gid_t group) {
    return 0; // No-op for Switch
}

uid_t geteuid(void) {
    return 0; // Always root on Switch
}
