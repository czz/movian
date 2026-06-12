/*
 * kvstore_switch.c - File-based key-value store for Nintendo Switch
 * Simple implementation using JSON-like text format instead of SQLite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "arch/threads.h"
#include "misc/str.h"
#include "main.h"
#include "db/kvstore.h"
#include "prop/prop.h"
#include "misc/rstr.h"

#define KVSTORE_MAX_ENTRIES 1000
#define KVSTORE_PATH "/switch/movian/kvstore.txt"

typedef struct kv_entry {
  char *key;
  char *value;
  int type; // KVSTORE_SET_STRING, KVSTORE_SET_INT, etc.
  struct kv_entry *next;
} kv_entry_t;

static kv_entry_t *kvstore_entries = NULL;
static hts_mutex_t kvstore_mutex;
static int kvstore_initialized = 0;

/**
 * Initialize kvstore
 */
void kvstore_init(void) {
  if(kvstore_initialized)
    return;
  
  hts_mutex_init(&kvstore_mutex);
  kvstore_initialized = 1;
  
  // Load from file if exists
  FILE *f = fopen(KVSTORE_PATH, "r");
  if(f) {
    char line[4096];
    while(fgets(line, sizeof(line), f)) {
      // Format: type|key|value
      char *type_str = strtok(line, "|");
      char *key = strtok(NULL, "|");
      char *value = strtok(NULL, "\n");
      
      if(type_str && key && value) {
        kv_entry_t *entry = calloc(1, sizeof(kv_entry_t));
        entry->key = strdup(key);
        entry->value = strdup(value);
        entry->type = atoi(type_str);
        entry->next = kvstore_entries;
        kvstore_entries = entry;
      }
    }
    fclose(f);
  }
}

/**
 * Finalize kvstore
 */
void kvstore_fini(void) {
  if(!kvstore_initialized)
    return;
  
  hts_mutex_lock(&kvstore_mutex);
  
  // Save to file
  FILE *f = fopen(KVSTORE_PATH, "w");
  if(f) {
    kv_entry_t *entry = kvstore_entries;
    while(entry) {
      fprintf(f, "%d|%s|%s\n", entry->type, entry->key, entry->value);
      entry = entry->next;
    }
    fclose(f);
  }
  
  // Free entries
  kv_entry_t *entry = kvstore_entries;
  while(entry) {
    kv_entry_t *next = entry->next;
    free(entry->key);
    free(entry->value);
    free(entry);
    entry = next;
  }
  kvstore_entries = NULL;
  
  hts_mutex_unlock(&kvstore_mutex);
  kvstore_initialized = 0;
}

/**
 * Find entry by key
 */
static kv_entry_t *kv_find_entry(const char *key) {
  kv_entry_t *entry = kvstore_entries;
  while(entry) {
    if(strcmp(entry->key, key) == 0)
      return entry;
    entry = entry->next;
  }
  return NULL;
}

/**
 * Get string value
 */
struct rstr *kv_url_opt_get_rstr(const char *url, int domain, const char *key) {
  if(!kvstore_initialized)
    kvstore_init();
  
  char full_key[512];
  snprintf(full_key, sizeof(full_key), "%d:%s:%s", domain, url, key);
  
  hts_mutex_lock(&kvstore_mutex);
  kv_entry_t *entry = kv_find_entry(full_key);
  const char *value = entry ? entry->value : NULL;
  struct rstr *rstr_val = value ? rstr_alloc(value) : NULL;
  hts_mutex_unlock(&kvstore_mutex);
  
  return rstr_val;
}

/**
 * Get int value
 */
int kv_url_opt_get_int(const char *url, int domain, const char *key, int def) {
  struct rstr *rstr_val = kv_url_opt_get_rstr(url, domain, key);
  if(rstr_val) {
    int val = atoi(rstr_val);
    rstr_release(rstr_val);
    return val;
  }
  return def;
}

/**
 * Get int64 value
 */
int64_t kv_url_opt_get_int64(const char *url, int domain, const char *key, int64_t def) {
  struct rstr *rstr_val = kv_url_opt_get_rstr(url, domain, key);
  if(rstr_val) {
    int64_t val = atoll(rstr_val);
    rstr_release(rstr_val);
    return val;
  }
  return def;
}

/**
 * Set value
 */
void kv_url_opt_set(const char *url, int domain, const char *key, int type, ...) {
  if(!kvstore_initialized)
    kvstore_init();
  
  char full_key[512];
  snprintf(full_key, sizeof(full_key), "%d:%s:%s", domain, url, key);
  
  char value_str[256];
  va_list ap;
  va_start(ap, type);
  
  switch(type) {
  case KVSTORE_SET_STRING:
    snprintf(value_str, sizeof(value_str), "%s", va_arg(ap, char *));
    break;
  case KVSTORE_SET_INT:
    snprintf(value_str, sizeof(value_str), "%d", va_arg(ap, int));
    break;
  case KVSTORE_SET_INT64:
    snprintf(value_str, sizeof(value_str), "%ld", va_arg(ap, int64_t));
    break;
  case KVSTORE_SET_VOID:
    strcpy(value_str, "void");
    break;
  default:
    strcpy(value_str, "");
    break;
  }
  
  va_end(ap);
  
  hts_mutex_lock(&kvstore_mutex);
  
  kv_entry_t *entry = kv_find_entry(full_key);
  if(entry) {
    free(entry->value);
    entry->value = strdup(value_str);
    entry->type = type;
  } else {
    entry = calloc(1, sizeof(kv_entry_t));
    entry->key = strdup(full_key);
    entry->value = strdup(value_str);
    entry->type = type;
    entry->next = kvstore_entries;
    kvstore_entries = entry;
  }
  
  hts_mutex_unlock(&kvstore_mutex);
}

/**
 * Deferred flush (no-op for file-based, writes on fini)
 */
void kvstore_deferred_flush(void) {
  // File-based implementation writes on fini
}

/**
 * Bind property to kvstore (stub for now)
 */
void kv_prop_bind_create(struct prop *p, const char *url) {
  // TODO: Implement property binding if needed
}
