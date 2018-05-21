#include <stdbool.h>
#include <stdint.h>
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include <list.h>
struct frame{
  struct thread* thd;
  void* kpage;
  struct s_page* page;
  struct list_elem frame_elem;
  struct lock lock;
};

void frameTable_init();
struct frame* frame_acquire(struct s_page*);
struct frame* frame_evict();

void frame_free(struct frame*);
void frameTable_free();

struct list frameTable;
