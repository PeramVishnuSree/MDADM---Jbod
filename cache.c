#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
static int cache_exists = 0;
static int cache_entries = 0;

int cache_create(int num_entries) {

  if (cache_exists != 0) {
    return -1;
  }

  if (num_entries > 4096 || num_entries < 2) {
    return -1;
  }

  cache_size = num_entries;
  cache =  malloc(num_entries*sizeof(cache_entry_t));
  cache_exists = 1;

  return 1;
}

int cache_destroy(void) {

  if (cache_exists == 0) {
    return -1;
  }
  
  free(cache);
  cache = NULL;
  cache_size = 0;
  cache_exists = 0;

  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {

  if (cache_exists == 0) {
    return -1;
  }

  if (cache_entries == 0) {
    return -1;
  }

  if (buf == NULL) {
    return -1;
  }

  num_queries += 1;
  clock += 1;

  for (int i = 0; i <= cache_entries;){
    
    if ((cache[i].disk_num) == disk_num && (cache[i].block_num) == block_num){
      
      if ((cache[i].block) == NULL) {
        return -1;
      }
      
      memcpy(buf, cache[i].block, 256);

      num_hits += 1;
      cache[i].access_time = clock;
      return 1;
      
    }

    i = i+1;

  }
  
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {

  for (int i = 0; i < cache_entries;) {
    if (cache[i].block_num == block_num && cache[i].disk_num == disk_num) {
      clock +=1;
      memcpy(cache[i].block, buf, 256);
      cache[i].access_time = clock;
    }
    i ++;
  }

}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  
  if (cache_exists == 0) {
    return -1;
  }

  if (buf == NULL) {
    return -1;
  }

  if (disk_num >15 || disk_num < 0) {
    return -1;
  }

  if (block_num < 0 || block_num > 255) {
    return -1;
  }

  clock +=1;

  // we have to check if the passed disk and block numebrs are already in the cache
  for (int m = 0; m < cache_entries;) {
    if (cache[m].block_num == block_num && cache[m].disk_num == disk_num) {
      return -1;
    }
    m +=1;
  }

  // we have to check if there is any space left in the cache: using cache entries variable
  // implement 2 for loops inside 2 different conditionals, the ocnditionals being 
  // cache_entries < cache_size, and cache_entries >= cache_size

  // we will need 2 for loops here, atleast for now

  if (cache_entries >= cache_size) {

    int LRU_time = cache[0].access_time;

      for (int i = 1; i < cache_size; i++) {
        if (cache[i].access_time < LRU_time) {
        LRU_time = cache[i].access_time;
      }
    }

    for (int j = 0; j < cache_size; j ++) {
      if (cache[j].access_time == LRU_time) {

        cache[j].valid = true;
        cache[j].disk_num  = disk_num;
        cache[j].block_num = block_num;
        memcpy(cache[j].block, buf, 256);
        cache[j].access_time = clock;

        break;
      }
    }
  }

  // if there is still space left in cache
  else {

        cache[cache_entries].valid = 1;
        cache[cache_entries].disk_num  = disk_num;
        cache[cache_entries].block_num = block_num;
        memcpy(cache[cache_entries].block, buf, 256);
        cache[cache_entries].access_time = clock;

        cache_entries += 1;
  }
  return 1;
}

bool cache_enabled(void) {
  if (cache_exists == 1){
    return true;
  }
  else{
    return false;
  }
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
