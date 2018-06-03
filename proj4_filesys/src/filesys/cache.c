#include <string.h>
#include <debug.h>
#include "cache.h"
#include "filesys.h"
#include "threads/synch.h"

struct lock buffer_cache_lock;
struct list_elem* last_entry;
//struct cache_entry buffer_cache[BUFFER_CACHE_SIZE];


void buffer_cache_init(){
  lock_init(&buffer_cache_lock);
  list_init(&cache_list);
  for(int i=0;i<BUFFER_CACHE_SIZE;i++){
    struct cache_entry* tmp = malloc(sizeof(struct cache_entry));
    tmp->free = true;
    tmp->accessed = true;
    tmp->dirty = false;
    //tmp->sector_disk = (block_sector_t) -1;
    memset(tmp->buffer,0,BLOCK_SECTOR_SIZE);
    list_push_back(&cache_list,&tmp->cache_elem);

    // buffer_cache[i].free = true;
    // buffer_cache[i].accessed = true;
    // buffer_cache[i].dirty = false;
    // buffer_cache[i].sector_disk = (block_sector_t)-1;
  }
  last_entry = list_begin(&cache_list);

}

void cache_entry_writeback(struct cache_entry* entry){
  block_write(fs_device,entry->sector_disk,entry->buffer);
  entry->dirty = false;
  entry->accessed = true;
}
//
//
void buffer_cache_writeback(){
  lock_acquire(&buffer_cache_lock);
  struct list_elem* e;
  while(!list_empty(&cache_list)){
    e = list_pop_front(&cache_list);
    struct cache_entry* entry = list_entry(e,struct cache_entry,cache_elem);
    if(entry->dirty)cache_entry_writeback(entry);
    free(entry);
  }
  lock_release(&buffer_cache_lock);
}

struct cache_entry* cache_lookup(block_sector_t sector){
  struct list_elem* e;
  for(e =list_begin(&cache_list);e!=list_end(&cache_list);e=list_next(e)){
    struct cache_entry* entry = list_entry(e,struct cache_entry,cache_elem);
    if(entry->sector_disk==sector&&!entry->free){
      //printf("cache hit\n");
      //printf("%d\n",sector);
      entry->accessed = true;
      return entry;
    }
  }
  return NULL;
}

struct cache_entry* cache_evict(){
  //PANIC("EVICT");
  struct list_elem* e;
  for(e =last_entry;e!=list_end(&cache_list);e=list_next(e)){
    struct cache_entry* entry = list_entry(e,struct cache_entry,cache_elem);
    if(!entry->accessed){
      entry->accessed = true;
      if(entry->dirty) cache_entry_writeback(entry);
      last_entry = list_next(e)==list_end(&cache_list)?list_begin(&cache_list):list_next(e);
      return entry;
    }else entry->accessed = false;
  }

  for(e =list_begin(&cache_list);e!=list_end(&cache_list);e=list_next(e)){
    struct cache_entry* entry = list_entry(e,struct cache_entry,cache_elem);
    if(!entry->accessed){
      entry->accessed = true;
      if(entry->dirty) cache_entry_writeback(entry);
      last_entry = list_next(e)==list_end(&cache_list)?list_begin(&cache_list):list_next(e);
      return entry;
    }else entry->accessed = false;
  }

  //PANIC("SHOULD HAVE RETURNED!");
  return NULL;
}

struct cache_entry* cache_acquire(){
  struct list_elem* e;
  for(e =list_begin(&cache_list);e!=list_end(&cache_list);e=list_next(e)){
    struct cache_entry* entry = list_entry(e,struct cache_entry,cache_elem);
    if(entry->free){
      //printf("find free\n");
      entry->free = false;

      return entry;
    }
  }
  //no free cache_entry

  return cache_evict();
}

struct cache_entry* cache_get(block_sector_t sector){
  //printf("%d\n", sector);
  //ASSERT(sector<10);
  lock_acquire(&buffer_cache_lock);
  struct cache_entry* entry = cache_lookup(sector);
  if(entry==NULL){
    //printf("cache miss\n");
    entry = cache_acquire();
    //printf("%d\n",idx );
    entry->sector_disk = sector;

    block_read(fs_device,sector,entry->buffer);
  }
  //block_read(fs_device,sector,entry->buffer);
  //printf("cache idx %d\n",entry->idx );

  //uint8_t tmp[512];
  //block_read(fs_device,sector,tmp);
  //block_read(fs_device,buffer_cache[idx].sector_disk,tar);
  //block_read(fs_device,sector,buffer_cache[0].buffer);
  //memcpy(tar,entry->buffer,BLOCK_SECTOR_SIZE);
  //memcpy(tar,tmp,BLOCK_SECTOR_SIZE);

  lock_release(&buffer_cache_lock);
  return entry;
}

// void cache_write(block_sector_t sector,void* src){
//   int idx;
//   lock_acquire(&buffer_cache_lock);
//   idx = cache_lookup(sector);
//   if(idx==-1){
//     idx = cache_evict();
//     buffer_cache[idx].sector_disk = sector;
//   }
//   buffer_cache[idx].dirty = true;
//   memcpy(buffer_cache[idx].buffer,src,BLOCK_SECTOR_SIZE);
//   lock_release(&buffer_cache_lock);
//   return;
// }
