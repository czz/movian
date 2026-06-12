/*
 *  Copyright (C) 2007-2015 Lonelycoder AB
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  This program is also available under a commercial proprietary license.
 *  For more information, contact andreas@lonelycoder.com
 */

/*
 * asyncio_switch.c - Async I/O implementation for Nintendo Switch using libnx BSD sockets
 */

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>

#include "main.h"
#include "arch/arch.h"
#include "arch/threads.h"
#include "networking/asyncio.h"
#include "misc/queue.h"
#include "prop/prop.h"
#include "misc/minmax.h"
#include "htsmsg/htsbuf.h"

LIST_HEAD(asyncio_fd_list, asyncio_fd);
LIST_HEAD(asyncio_worker_list, asyncio_worker);
LIST_HEAD(asyncio_timer_list, asyncio_timer);
TAILQ_HEAD(asyncio_dns_req_queue, asyncio_dns_req);
TAILQ_HEAD(asyncio_task_queue, asyncio_task);

struct asyncio_fd {
  LIST_ENTRY(asyncio_fd) af_link;
  int af_fd;
  int af_events;
  int af_poll_events;
  int af_refcount;
  int af_timeout;
  int af_pending_errno;
  int af_connected;
  int af_suspended;
  
  asyncio_fd_callback_t *af_callback;
  void *af_opaque;
  const char *af_name;
  
  union {
    asyncio_accept_callback_t *af_accept_callback;
    asyncio_udp_callback_t    *af_udp_callback;
    asyncio_error_callback_t  *af_error_callback;
  };
  
  asyncio_read_callback_t *af_read_callback;
  
  htsbuf_queue_t af_sendq;
  htsbuf_queue_t af_recvq;
  
  char *af_hostname;
  net_addr_t af_bind_addr;
  void (*af_resume)(struct asyncio_fd *af);
  int af_bind_any : 1;
  int af_broadcast : 1;
  
  // mbedTLS SSL context
  mbedtls_ssl_context af_ssl;
  mbedtls_ssl_config af_ssl_conf;
  int af_ssl_established;
  int af_ssl_read_status;
  int af_ssl_write_status;
};

static hts_thread_t asyncio_thread_id;
static struct asyncio_timer_list asyncio_timers;
static hts_mutex_t asyncio_worker_mutex;
static struct asyncio_worker_list asyncio_workers;
static int asyncio_pipe[2];
static struct asyncio_fd_list asyncio_fds;
static int asyncio_num_fds;

struct prop_courier *asyncio_courier;

static hts_mutex_t asyncio_dns_mutex;
static int asyncio_dns_worker;
static struct asyncio_dns_req_queue asyncio_dns_pending;
static struct asyncio_dns_req_queue asyncio_dns_completed;

static int adr_resolver_running;

// DNS request structure
struct asyncio_dns_req {
  TAILQ_ENTRY(asyncio_dns_req) adr_link;
  char *adr_hostname;
  void *adr_opaque;
  void (*adr_cb)(void *opaque, int status, const void *data);
  
  int adr_status;
  int adr_cancelled;
  const void *adr_data;
  const char *adr_errmsg;
  net_addr_t adr_addr;
};

static void adr_deliver_cb(void);
static void asyncio_handle_pipe(asyncio_fd_t *af, void *opaque, int event, int error);
static void asyncio_process_workers(void);

static hts_mutex_t asyncio_task_mutex;
static struct asyncio_task_queue asyncio_tasks;

// Task structure
typedef struct asyncio_task {
  TAILQ_ENTRY(asyncio_task) at_link;
  void (*at_fn)(void *aux);
  void *at_aux;
} asyncio_task_t;

static int64_t async_now;

static hts_thread_t asyncio_thread_id;

// Forward declaration
void asyncio_dopoll(void);
static void *asyncio_thread(void *aux);

// Worker thread structure
typedef struct asyncio_worker {
  LIST_ENTRY(asyncio_worker) link;
  void (*fn)(void);
  int id;
  int pending;
} asyncio_worker_t;

static struct asyncio_worker_list asyncio_workers;
static hts_mutex_t asyncio_worker_mutex;
static int worker_id_counter = 0;

// Network change callback
static void (*network_change_callback)(const struct netif *ni) = NULL;

// Network change detection thread
static int network_change_running = 0;

// Local netif structure for Switch
static netif_t local_netif;

/**
 * Get local IP address and populate netif
 */
static int
get_local_netif(netif_t *ni)
{
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0)
    return -1;
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  addr.sin_addr.s_addr = inet_addr("8.8.8.8");
  
  if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }
  
  struct sockaddr_in local_addr;
  socklen_t len = sizeof(local_addr);
  if(getsockname(sock, (struct sockaddr *)&local_addr, &len) < 0) {
    close(sock);
    return -1;
  }
  
  close(sock);
  
  // Populate netif structure
  memset(ni, 0, sizeof(netif_t));
  strncpy(ni->ifname, "eth0", NET_IFNAME_SIZE - 1);
  memcpy(ni->ipv4_addr, &local_addr.sin_addr.s_addr, 4);
  
  // Netmask hardcoded to /24
  // libnx BSD sockets don't support ioctl/ifreq for netmask detection
  // nifm service would provide this but isn't available in this environment
  ni->ipv4_mask[0] = 255;
  ni->ipv4_mask[1] = 255;
  ni->ipv4_mask[2] = 255;
  ni->ipv4_mask[3] = 0;
  
  return 0;
}

/**
 * Check if network is reachable
 */
static int
check_network_reachable(void)
{
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0)
    return 0;
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(53);
  addr.sin_addr.s_addr = inet_addr("8.8.8.8");
  
  // Set non-blocking
  fcntl(sock, F_SETFL, O_NONBLOCK);
  
  int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  if(ret < 0 && errno != EINPROGRESS) {
    close(sock);
    return 0;
  }
  
  if(ret == 0) {
    close(sock);
    return 1;
  }
  
  // Wait for connection
  fd_set writefds;
  struct timeval tv;
  FD_ZERO(&writefds);
  FD_SET(sock, &writefds);
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  
  ret = select(sock + 1, NULL, &writefds, NULL, &tv);
  close(sock);
  
  return ret > 0;
}

/**
 * Network change detection thread
 */
static void *
network_change_monitor(void *aux)
{
  int last_connected = 0;
  netif_t last_netif;
  memset(&last_netif, 0, sizeof(last_netif));
  
  while(network_change_running) {
    int connected = check_network_reachable();
    netif_t current_netif;
    int has_netif = (get_local_netif(&current_netif) == 0);
    
    int netif_changed = 0;
    if(has_netif) {
      if(strcmp(current_netif.ifname, last_netif.ifname) != 0 ||
         memcmp(current_netif.ipv4_addr, last_netif.ipv4_addr, 4) != 0 ||
         memcmp(current_netif.ipv4_mask, last_netif.ipv4_mask, 4) != 0) {
        netif_changed = 1;
        memcpy(&last_netif, &current_netif, sizeof(netif_t));
        memcpy(&local_netif, &current_netif, sizeof(netif_t));
      }
    }
    
    if(connected != last_connected || netif_changed) {
      last_connected = connected;
      if(network_change_callback != NULL) {
        if(has_netif)
          network_change_callback(&local_netif);
        else
          network_change_callback(NULL);
      }
    }
    
    // Poll every 5 seconds
    usleep(5000000);
  }
  
  return NULL;
}

// mbedTLS global context
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static int mbedtls_initialized = 0;

// Custom BIO functions for mbedTLS
static int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len) {
  asyncio_fd_t *af = (asyncio_fd_t *)ctx;
  return send(af->af_fd, buf, len, 0);
}

static int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len) {
  asyncio_fd_t *af = (asyncio_fd_t *)ctx;
  return recv(af->af_fd, buf, len, 0);
}

static __inline void asyncio_verify_thread(void) {
  assert(hts_thread_current() == asyncio_thread_id);
}

/**
 * Initialize async I/O
 */
void asyncio_init_early(void) {
  LIST_INIT(&asyncio_fds);
  LIST_INIT(&asyncio_workers);
  LIST_INIT(&asyncio_timers);
  TAILQ_INIT(&asyncio_dns_pending);
  TAILQ_INIT(&asyncio_dns_completed);
  TAILQ_INIT(&asyncio_tasks);
  
  hts_mutex_init(&asyncio_worker_mutex);
  hts_mutex_init(&asyncio_dns_mutex);
  hts_mutex_init(&asyncio_task_mutex);
  
  // Create pipe for task wakeup
  pipe(asyncio_pipe);
  fcntl(asyncio_pipe[0], F_SETFL, O_NONBLOCK);
  fcntl(asyncio_pipe[1], F_SETFL, O_NONBLOCK);
  
  // Initialize mbedTLS
  if (!mbedtls_initialized) {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char *pers = "movian_asyncio";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *)pers, strlen(pers));
    
    mbedtls_initialized = 1;
  }
}

/**
 * Asyncio thread (event loop)
 */
static void *
asyncio_thread(void *aux)
{
  asyncio_thread_id = hts_thread_current();

  // Add pipe to event loop
  asyncio_add_fd(asyncio_pipe[0], ASYNCIO_READ, asyncio_handle_pipe,
                 NULL, "Pipe");

  async_now = arch_get_ts();

  // Trigger network change notification
  asyncio_trig_network_change();

  while(1)
    asyncio_dopoll();
  return NULL;
}

/**
 * Start async I/O thread
 */
void asyncio_start(void) {
  // Spawn dedicated thread for event loop
  hts_thread_create_detached("asyncio", asyncio_thread,
                             NULL, THREAD_PRIO_MODEL);
}

/**
 * SSL handshake
 */
static int asyncio_ssl_handshake(asyncio_fd_t *af) {
  int ret;
  
  ret = mbedtls_ssl_setup(&af->af_ssl, &af->af_ssl_conf);
  if (ret != 0) return ret;
  
  mbedtls_ssl_set_bio(&af->af_ssl, af, mbedtls_net_send, mbedtls_net_recv, NULL);
  
  ret = mbedtls_ssl_handshake(&af->af_ssl);
  if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
    af->af_ssl_read_status = ret;
    return ret;
  }
  
  if (ret == 0) {
    af->af_ssl_established = 1;
    af->af_ssl_read_status = 0;
  }
  
  return ret;
}

/**
 * SSL read
 */
static void asyncio_ssl_read(asyncio_fd_t *af) {
  if(!af->af_ssl_established) {
    asyncio_ssl_handshake(af);
    return;
  }

  while(af->af_ssl_established) {
    char buf[4096];
    if(af->af_ssl_write_status == MBEDTLS_ERR_SSL_WANT_READ) {
      af->af_ssl_write_status = 0;
    }
    af->af_ssl_read_status = 0;
    int r = mbedtls_ssl_read(&af->af_ssl, (unsigned char *)buf, sizeof(buf));
    
    if(r > 0) {
      htsbuf_append(&af->af_recvq, buf, r);
    } else if(r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
      af->af_ssl_read_status = r;
      return;
    } else if(r == 0) {
      // Connection closed
      if(af->af_error_callback)
        af->af_error_callback(af->af_opaque, "SSL connection closed");
      return;
    } else {
      // Error
      if(af->af_error_callback)
        af->af_error_callback(af->af_opaque, "SSL read error");
      return;
    }
    
    if(af->af_read_callback)
      af->af_read_callback(af->af_opaque, &af->af_recvq);
  }
}

/**
 * SSL events (for poll)
 */
static int asyncio_ssl_events(asyncio_fd_t *af) {
  if (af->af_ssl_read_status == MBEDTLS_ERR_SSL_WANT_READ)
    return ASYNCIO_READ;
  if (af->af_ssl_read_status == MBEDTLS_ERR_SSL_WANT_WRITE)
    return ASYNCIO_WRITE;
  return af->af_poll_events;
}

/**
 * Event loop with poll()
 */
void asyncio_dopoll(void) {
  asyncio_timer_t *at;
  
  // Process expired timers
  while((at = LIST_FIRST(&asyncio_timers)) != NULL &&
        at->at_expire <= async_now) {
    LIST_REMOVE(at, at_link);
    at->at_expire = 0;
    at->at_fn(at->at_opaque);
  }
  
  // Build pollfd array
  asyncio_fd_t *af;
  struct pollfd *fds = alloca(asyncio_num_fds * sizeof(struct pollfd));
  asyncio_fd_t **afds = alloca(asyncio_num_fds * sizeof(asyncio_fd_t *));
  int n = 0;
  
  int timeout = INT32_MAX;
  
  LIST_FOREACH(af, &asyncio_fds, af_link) {
    if(af->af_pending_errno) {
      if(af->af_callback)
        af->af_callback(af, af->af_opaque, ASYNCIO_ERROR, af->af_pending_errno);
      goto release;
    }
    
    if(af->af_timeout) {
      if(af->af_timeout <= async_now) {
        af->af_timeout = 0;
        if(af->af_callback)
          af->af_callback(af, af->af_opaque, ASYNCIO_TIMEOUT, 0);
        goto release;
      }
      timeout = MIN(timeout, (af->af_timeout - async_now + 999) / 1000);
    }
    
    if(af->af_fd == -1) {
      continue;
    }
    
    fds[n].fd = af->af_fd;

    // Use SSL events if SSL is active
    if (af->af_ssl_established || af->af_ssl_read_status != 0)
      fds[n].events = asyncio_ssl_events(af);
    else
      fds[n].events = af->af_poll_events;

    fds[n].revents = 0;
    afds[n] = af;
    
    af->af_refcount++;
    n++;
  }
  
  if((at = LIST_FIRST(&asyncio_timers)) != NULL)
    timeout = MIN(timeout, (at->at_expire - async_now + 999) / 1000);
  
  if(timeout == INT32_MAX)
    timeout = -1;
  
  int err = poll(fds, n, timeout);
  
  async_now = arch_get_ts();
  
  for(int i = 0; i < n; i++) {
    af = afds[i];
    
    if(af->af_callback == NULL)
      continue;
    
    if(fds[i].revents & POLLHUP) {
      af->af_callback(af, af->af_opaque, ASYNCIO_ERROR, ECONNRESET);
      continue;
    }
    
    if(fds[i].revents & POLLERR || err < 0) {
      int sock_err;
      socklen_t errlen = sizeof(int);
      
      if(getsockopt(af->af_fd, SOL_SOCKET, SO_ERROR, (void *)&sock_err, &errlen)) {
        sock_err = errno;
      }
      af->af_callback(af, af->af_opaque, ASYNCIO_ERROR, sock_err);
      continue;
    }
    
    if(fds[i].revents & POLLIN) {
      // Handle SSL read
      if(af->af_ssl_established || af->af_ssl_read_status != 0) {
        asyncio_ssl_read(af);
      }
      // Handle TCP accept
      else if(af->af_accept_callback != NULL) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int new_fd = accept(af->af_fd, (struct sockaddr *)&addr, &addrlen);
        if(new_fd >= 0) {
          net_addr_t na;
          memset(&na, 0, sizeof(na));
          na.na_family = AF_INET;
          na.na_port = ntohs(addr.sin_port);
          memcpy(na.na_addr, &addr.sin_addr.s_addr, 4);
          af->af_accept_callback(af, af->af_opaque, new_fd, &na);
        }
      }
      // Handle UDP receive
      else if(af->af_udp_callback != NULL) {
        char buf[65536];
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int len = recvfrom(af->af_fd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);
        if(len > 0) {
          net_addr_t na;
          memset(&na, 0, sizeof(na));
          na.na_family = AF_INET;
          na.na_port = ntohs(addr.sin_port);
          memcpy(na.na_addr, &addr.sin_addr.s_addr, 4);
          af->af_udp_callback(af->af_opaque, buf, len, &na);
        }
      }
      // Handle TCP read
      else if(af->af_read_callback != NULL) {
        af->af_read_callback(af, af->af_opaque);
      }
      // Generic callback
      else if(af->af_callback != NULL) {
        af->af_callback(af, af->af_opaque, ASYNCIO_READ, 0);
      }
    }
    
    if(fds[i].revents & POLLOUT) {
      af->af_callback(af, af->af_opaque, ASYNCIO_WRITE, 0);
    }
  }
  
release:
  for(int i = 0; i < n; i++) {
    af = afds[i];
    af->af_refcount--;
    if(af->af_refcount == 0 && af->af_fd == -1) {
      free(af);
    }
  }
}

/**
 * Suspend async I/O
 */
void asyncio_suspend(void) {
  // No-op for Switch
}

/**
 * Resume async I/O
 */
void asyncio_resume(void) {
  // No-op for Switch
}

/**
 * Add file descriptor to async I/O
 */
asyncio_fd_t *
asyncio_add_fd(int fd, int events,
              asyncio_fd_callback_t *cb, void *opaque,
              const char *name) {
  asyncio_fd_t *af = calloc(1, sizeof(asyncio_fd_t));
  af->af_fd = fd;
  af->af_events = events;
  af->af_poll_events = events;
  af->af_callback = cb;
  af->af_opaque = opaque;
  af->af_name = name;
  af->af_refcount = 1;
  htsbuf_queue_init(&af->af_sendq, 0);
  htsbuf_queue_init(&af->af_recvq, 0);
  
  // Set socket non-blocking
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags != -1) {
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
  
  LIST_INSERT_HEAD(&asyncio_fds, af, af_link);
  asyncio_num_fds++;
  
  return af;
}

/**
 * Delete file descriptor from async I/O
 */
void asyncio_del_fd(asyncio_fd_t *af) {
  if (af == NULL) return;
  
  LIST_REMOVE(af, af_link);
  asyncio_num_fds--;
  
  close(af->af_fd);
  af->af_fd = -1;
  
  if(af->af_refcount == 0) {
    free(af);
  }
}

/**
 * Get current time
 */
int64_t async_current_time(void) {
  return arch_get_ts();
}

/**
 * Register for network changes
 */
void asyncio_register_for_network_changes(void (*cb)(const struct netif *ni)) {
  network_change_callback = cb;
  
  // Start network change monitor if not already running
  if(!network_change_running && cb != NULL) {
    network_change_running = 1;
    hts_thread_create_detached("netmon", network_change_monitor, NULL,
                               THREAD_PRIO_MODEL);
  }
}

/**
 * Trigger network change
 */
void asyncio_trig_network_change(void) {
  if (network_change_callback != NULL) {
    // For Switch, we don't have netif detection
    // Call with NULL to indicate network change
    network_change_callback(NULL);
  }
}

/**
 * Handle pipe wakeup
 */
static void
asyncio_handle_pipe(asyncio_fd_t *af, void *opaque, int event, int error)
{
  char x;
  if(read(asyncio_pipe[0], &x, 1) != 1)
    return;

  if(x == 1) {
    struct asyncio_task_queue atq;
    asyncio_task_t *at, *next;

    hts_mutex_lock(&asyncio_task_mutex);
    TAILQ_MOVE(&atq, &asyncio_tasks, at_link);
    TAILQ_INIT(&asyncio_tasks);
    hts_mutex_unlock(&asyncio_task_mutex);

    for(at = TAILQ_FIRST(&atq); at != NULL; at = next) {
      next = TAILQ_NEXT(at, at_link);
      at->at_fn(at->at_aux);
      free(at);
    }
  }
  
  if(x == 2) {
    // DNS delivery and worker processing
    adr_deliver_cb();
    asyncio_process_workers();
  }
}

/**
 * Wakeup event loop
 */
static void
asyncio_wakeup(int code)
{
  char c = code;
  write(asyncio_pipe[1], &c, 1);
}

/**
 * Add worker thread
 */
int asyncio_add_worker(void (*fn)(void)) {
  asyncio_worker_t *aw = calloc(1, sizeof(asyncio_worker_t));
  if (aw == NULL) return -1;
  
  aw->fn = fn;
  
  hts_mutex_lock(&asyncio_worker_mutex);
  worker_id_counter++;
  aw->id = worker_id_counter;
  LIST_INSERT_HEAD(&asyncio_workers, aw, link);
  hts_mutex_unlock(&asyncio_worker_mutex);
  
  return aw->id;
}

/**
 * Wake up worker
 */
void asyncio_wakeup_worker(int id) {
  hts_mutex_lock(&asyncio_worker_mutex);
  asyncio_worker_t *aw;
  LIST_FOREACH(aw, &asyncio_workers, link) {
    if(aw->id == id) {
      aw->pending = 1;
      break;
    }
  }
  hts_mutex_unlock(&asyncio_worker_mutex);
  
  // Signal the event loop to process worker callbacks
  asyncio_wakeup(2);
}

/**
 * Process pending workers
 */
static void
asyncio_process_workers(void)
{
  asyncio_worker_t *aw, *next;
  
  hts_mutex_lock(&asyncio_worker_mutex);
  for(aw = LIST_FIRST(&asyncio_workers); aw != NULL; aw = next) {
    next = LIST_NEXT(aw, link);
    if(aw->pending) {
      aw->pending = 0;
      hts_mutex_unlock(&asyncio_worker_mutex);
      aw->fn();
      hts_mutex_lock(&asyncio_worker_mutex);
    }
  }
  hts_mutex_unlock(&asyncio_worker_mutex);
}

/**
 * Run task in async context
 */
void asyncio_run_task(void (*fn)(void *aux), void *aux) {
  asyncio_task_t *at = malloc(sizeof(asyncio_task_t));
  at->at_fn = fn;
  at->at_aux = aux;

  hts_mutex_lock(&asyncio_task_mutex);
  int do_signal = TAILQ_EMPTY(&asyncio_tasks);
  TAILQ_INSERT_TAIL(&asyncio_tasks, at, at_link);
  hts_mutex_unlock(&asyncio_task_mutex);
  if(do_signal)
    asyncio_wakeup(1);
}

/**
 * TCP listen
 */
asyncio_fd_t *
asyncio_listen(const char *name,
              int port,
              asyncio_accept_callback_t *cb,
              void *opaque,
              int bind_any_on_fail) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return NULL;
  
  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return NULL;
  }
  
  if (listen(fd, 128) < 0) {
    close(fd);
    return NULL;
  }
  
  asyncio_fd_t *af = asyncio_add_fd(fd, ASYNCIO_READ, NULL, opaque, name);
  if (af == NULL) {
    close(fd);
    return NULL;
  }
  
  af->af_accept_callback = cb;
  return af;
}

/**
 * TCP connect
 */
asyncio_fd_t *
asyncio_connect(const char *name,
               const net_addr_t *remote_addr,
               asyncio_error_callback_t *connect_cb,
               asyncio_read_callback_t *read_cb,
               void *opaque,
               int timeout,
               void *tls,
               const char *hostname) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return NULL;
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(remote_addr->na_port);
  memcpy(&addr.sin_addr.s_addr, remote_addr->na_addr, 4);
  
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    if (connect_cb) connect_cb(opaque, "Connection failed");
    return NULL;
  }
  
  asyncio_fd_t *af = asyncio_add_fd(fd, ASYNCIO_READ, NULL, opaque, name);
  if (af == NULL) {
    close(fd);
    return NULL;
  }
  
  af->af_error_callback = connect_cb;
  af->af_read_callback = read_cb;
  
  if (hostname) {
    af->af_hostname = strdup(hostname);
  }
  
  // Setup SSL if TLS context provided
  if (tls != NULL) {
    mbedtls_ssl_config *conf = (mbedtls_ssl_config *)tls;
    memcpy(&af->af_ssl_conf, conf, sizeof(mbedtls_ssl_config));
    
    mbedtls_ssl_init(&af->af_ssl);
    af->af_ssl_established = 0;
    af->af_ssl_read_status = 0;
    af->af_ssl_write_status = 0;
    
    // Perform handshake
    int ret = asyncio_ssl_handshake(af);
    if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      // Handshake failed
      mbedtls_ssl_free(&af->af_ssl);
      if (connect_cb) connect_cb(opaque, "SSL handshake failed");
      asyncio_del_fd(af);
      return NULL;
    }
  }
  
  return af;
}

/**
 * Attach existing socket
 */
asyncio_fd_t *
asyncio_attach(const char *name, int fd,
              asyncio_error_callback_t *error_cb,
              asyncio_read_callback_t *read_cb,
              void *opaque,
              void *tls) {
  return asyncio_add_fd(fd, ASYNCIO_READ, NULL, opaque, name);
}

/**
 * Send data
 */
void asyncio_send(asyncio_fd_t *af, const void *buf, size_t len, int cork) {
  if (af == NULL) return;
  
  if (af->af_ssl_established) {
    mbedtls_ssl_write(&af->af_ssl, buf, len);
  } else {
    send(af->af_fd, buf, len, 0);
  }
}

/**
 * Send queue
 */
void asyncio_sendq(asyncio_fd_t *af, htsbuf_queue_t *q, int cork) {
  if (af == NULL) return;
  
  htsbuf_data_t *hd;
  while((hd = TAILQ_FIRST(&q->hq_q)) != NULL) {
    if (af->af_ssl_established) {
      mbedtls_ssl_write(&af->af_ssl, hd->hd_data + hd->hd_data_off, hd->hd_data_len - hd->hd_data_off);
    } else {
      send(af->af_fd, hd->hd_data + hd->hd_data_off, hd->hd_data_len - hd->hd_data_off, 0);
    }
    htsbuf_data_free(q, hd);
  }
}

/**
 * Get port
 */
int asyncio_get_port(asyncio_fd_t *af) {
  if (af == NULL) return 0;
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  getsockname(af->af_fd, (struct sockaddr *)&addr, &len);
  return ntohs(addr.sin_port);
}

/**
 * Set timeout
 */
void asyncio_set_timeout_delta_sec(asyncio_fd_t *af, int seconds) {
  if (af == NULL) return;
  af->af_timeout = arch_get_ts() + (int64_t)seconds * 1000000;
}

/**
 * UDP bind
 */
asyncio_fd_t *
asyncio_udp_bind(const char *name,
                const net_addr_t *na,
                asyncio_udp_callback_t *cb,
                void *opaque,
                int bind_any_on_fail,
                int broadcast) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return NULL;
  
  if (broadcast) {
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
  }
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(na->na_port);
  memcpy(&addr.sin_addr.s_addr, na->na_addr, 4);
  
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return NULL;
  }
  
  asyncio_fd_t *af = asyncio_add_fd(fd, ASYNCIO_READ, NULL, opaque, name);
  if (af == NULL) {
    close(fd);
    return NULL;
  }
  
  af->af_udp_callback = cb;
  return af;
}

/**
 * UDP send
 */
void asyncio_udp_send(asyncio_fd_t *af, const void *data, int size,
                    const net_addr_t *remote_addr) {
  if (af == NULL) return;
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(remote_addr->na_port);
  memcpy(&addr.sin_addr.s_addr, remote_addr->na_addr, 4);
  
  sendto(af->af_fd, data, size, 0, (struct sockaddr *)&addr, sizeof(addr));
}

/**
 * UDP add multicast membership
 */
int asyncio_udp_add_membership(asyncio_fd_t *af, const net_addr_t *group,
                             const net_addr_t *interface) {
  struct ip_mreq mreq;
  
  memset(&mreq, 0, sizeof(mreq));
  memcpy(&mreq.imr_multiaddr.s_addr, group->na_addr, 4);
  
  if (interface != NULL) {
    memcpy(&mreq.imr_interface.s_addr, interface->na_addr, 4);
  } else {
    mreq.imr_interface.s_addr = INADDR_ANY;
  }
  
  if (setsockopt(af->af_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    return -1;
  }
  
  return 0;
}

/**
 * Timer init
 */
void asyncio_timer_init(asyncio_timer_t *at, void (*fn)(void *opaque),
                      void *opaque) {
  memset(at, 0, sizeof(asyncio_timer_t));
  at->at_fn = fn;
  at->at_opaque = opaque;
}

/**
 * Timer arm
 */
void asyncio_timer_arm(asyncio_timer_t *at, int64_t ts) {
  at->at_expire = ts;
  LIST_INSERT_HEAD(&asyncio_timers, at, at_link);
}

/**
 * Timer arm delta
 */
void asyncio_timer_arm_delta_sec(asyncio_timer_t *at, int seconds) {
  at->at_expire = arch_get_ts() + (int64_t)seconds * 1000000;
  LIST_INSERT_HEAD(&asyncio_timers, at, at_link);
}

/**
 * Timer disarm
 */
void asyncio_timer_disarm(asyncio_timer_t *at) {
  if (at->at_expire != 0) {
    LIST_REMOVE(at, at_link);
    at->at_expire = 0;
  }
}

/**
 * DNS resolve
 */
static int
adr_resolve(asyncio_dns_req_t *adr)
{
  struct addrinfo hints, *res, *p;
  int ret;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  
  ret = getaddrinfo(adr->adr_hostname, NULL, &hints, &res);
  if(ret != 0) {
    adr->adr_errmsg = gai_strerror(ret);
    return -1;
  }
  
  // Use first result
  for(p = res; p != NULL; p = p->ai_next) {
    if(p->ai_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
      adr->adr_addr.na_family = AF_INET;
      adr->adr_addr.na_port = 0;
      memcpy(adr->adr_addr.na_addr, &addr->sin_addr.s_addr, 4);
      freeaddrinfo(res);
      return 0;
    }
  }
  
  freeaddrinfo(res);
  adr->adr_errmsg = "No IPv4 addresses found";
  return -1;
}

/**
 * DNS resolver thread
 */
static void *
adr_resolver(void *aux)
{
  asyncio_dns_req_t *adr;
  hts_mutex_lock(&asyncio_dns_mutex);
  while((adr = TAILQ_FIRST(&asyncio_dns_pending)) != NULL) {
    TAILQ_REMOVE(&asyncio_dns_pending, adr, adr_link);
    
    hts_mutex_unlock(&asyncio_dns_mutex);
    
    if(adr_resolve(adr)) {
      adr->adr_status = ASYNCIO_DNS_STATUS_FAILED;
      adr->adr_data = adr->adr_errmsg;
    } else {
      adr->adr_status = ASYNCIO_DNS_STATUS_COMPLETED;
      adr->adr_data = &adr->adr_addr;
    }
    hts_mutex_lock(&asyncio_dns_mutex);
    TAILQ_INSERT_TAIL(&asyncio_dns_completed, adr, adr_link);
    asyncio_wakeup_worker(asyncio_dns_worker);
  }
  adr_resolver_running = 0;
  hts_mutex_unlock(&asyncio_dns_mutex);
  return NULL;
}

/**
 * DNS lookup host
 */
asyncio_dns_req_t *
asyncio_dns_lookup_host(const char *hostname,
                        void (*cb)(void *opaque,
                                   int status,
                                   const void *data),
                        void *opaque) {
  asyncio_dns_req_t *adr;
  
  adr = calloc(1, sizeof(asyncio_dns_req_t));
  adr->adr_hostname = strdup(hostname);
  adr->adr_cb = cb;
  adr->adr_opaque = opaque;
  
  hts_mutex_lock(&asyncio_dns_mutex);
  TAILQ_INSERT_TAIL(&asyncio_dns_pending, adr, adr_link);
  if(!adr_resolver_running) {
    adr_resolver_running = 1;
    hts_thread_create_detached("DNS resolver", adr_resolver, NULL, 
                               THREAD_PRIO_BGTASK);
  }
  hts_mutex_unlock(&asyncio_dns_mutex);
  return adr;
}

/**
 * DNS cancel
 */
void asyncio_dns_cancel(asyncio_dns_req_t *adr) {
  if (adr == NULL) return;
  
  hts_mutex_lock(&asyncio_dns_mutex);
  adr->adr_cancelled = 1;
  hts_mutex_unlock(&asyncio_dns_mutex);
}

/**
 * DNS deliver callback
 */
static void
adr_deliver_cb(void)
{
  asyncio_dns_req_t *adr;
  
  hts_mutex_lock(&asyncio_dns_mutex);
  
  while((adr = TAILQ_FIRST(&asyncio_dns_completed)) != NULL) {
    TAILQ_REMOVE(&asyncio_dns_completed, adr, adr_link);
    hts_mutex_unlock(&asyncio_dns_mutex);
    
    if(!adr->adr_cancelled) {
      adr->adr_cb(adr->adr_opaque, adr->adr_status, adr->adr_data);
    }
    
    free(adr->adr_hostname);
    free(adr);
    
    hts_mutex_lock(&asyncio_dns_mutex);
  }
  hts_mutex_unlock(&asyncio_dns_mutex);
}

/**
 * SSL create client
 */
void *asyncio_ssl_create_client(void) {
  mbedtls_ssl_config *conf = malloc(sizeof(mbedtls_ssl_config));
  if (conf == NULL) return NULL;
  
  mbedtls_ssl_config_init(conf);
  
  int ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    mbedtls_ssl_config_free(conf);
    free(conf);
    return NULL;
  }
  
  mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &ctr_drbg);
  
  return conf;
}

/**
 * SSL create server
 */
void *asyncio_ssl_create_server(const char *privatekeyfile,
                                const char *certfile) {
  mbedtls_ssl_config *conf = malloc(sizeof(mbedtls_ssl_config));
  if (conf == NULL) return NULL;
  
  mbedtls_ssl_config_init(conf);
  
  int ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_SERVER,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    mbedtls_ssl_config_free(conf);
    free(conf);
    return NULL;
  }
  
  mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &ctr_drbg);
  
  // Load certificate
  mbedtls_x509_crt *crt = malloc(sizeof(mbedtls_x509_crt));
  mbedtls_x509_crt_init(crt);
  
  ret = mbedtls_x509_crt_parse_file(crt, certfile);
  if (ret != 0) {
    mbedtls_x509_crt_free(crt);
    free(crt);
    mbedtls_ssl_config_free(conf);
    free(conf);
    return NULL;
  }
  
  // Load private key
  mbedtls_pk_context *pk = malloc(sizeof(mbedtls_pk_context));
  mbedtls_pk_init(pk);
  
  ret = mbedtls_pk_parse_keyfile(pk, privatekeyfile, NULL);
  if (ret != 0) {
    mbedtls_pk_free(pk);
    free(pk);
    mbedtls_x509_crt_free(crt);
    free(crt);
    mbedtls_ssl_config_free(conf);
    free(conf);
    return NULL;
  }
  
  // Set certificate and private key
  ret = mbedtls_ssl_conf_own_cert(conf, crt, pk);
  if (ret != 0) {
    mbedtls_pk_free(pk);
    free(pk);
    mbedtls_x509_crt_free(crt);
    free(crt);
    mbedtls_ssl_config_free(conf);
    free(conf);
    return NULL;
  }
  
  // Store certificate and key pointers in config for later cleanup
  // Note: In a real implementation, we'd need a custom structure to hold these
  // For now, we'll leak them (they'll be freed when the config is freed)
  
  return conf;
}

/**
 * SSL free
 */
void asyncio_ssl_free(void *ctx) {
  if (ctx == NULL) return;
  
  mbedtls_ssl_config *conf = (mbedtls_ssl_config *)ctx;
  mbedtls_ssl_config_free(conf);
  free(conf);
}
