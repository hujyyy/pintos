#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <stdbool.h>
#include <stdint.h>
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "filesys/off_t.h"
#include "frame.h"
#include <hash.h>
#include <list.h>
#include "devices/block.h"


struct s_page{
  void* vaddr;
  struct frame* frame;
  bool writable;

  struct thread* thd;
  struct file* file;
  off_t file_ofs;
  off_t file_readsz;

  bool swapped;
  block_sector_t sector_idx;

  struct hash_elem page_elem;

};
hash_less_func page_less;
hash_hash_func page_hash;
void page_destroy(struct hash_elem*,void* aux UNUSED);

struct s_page* page_alloc(void* vaddr, bool writable);
struct s_page* page_find(void* vaddr,void*);
bool page_map(struct s_page*);
bool page_out (struct s_page *p);
void page_dealloc(struct s_page* pg);
void pageTable_free(void);
#endif
