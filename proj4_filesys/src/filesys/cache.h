#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include <list.h>

#define BUFFER_CACHE_SIZE 64

struct cache_entry{
  bool free;
  bool accessed;
  bool dirty;
  struct list_elem cache_elem;

  uint8_t buffer[BLOCK_SECTOR_SIZE];
  block_sector_t sector_disk;


};



void buffer_cache_init(void);
void cache_entry_writeback(struct cache_entry* entry);
void buffer_cache_writeback(void);
struct cache_entry* cache_lookup(block_sector_t sector);
struct cache_entry* cache_evict(void);
struct cache_entry* cache_acquire(void);
struct cache_entry* cache_get(block_sector_t sector);
void cache_write(block_sector_t sector,void* src);


//struct cache_entry buffer_cache[BUFFER_CACHE_SIZE];
struct list cache_list;
