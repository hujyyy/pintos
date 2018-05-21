#include "page.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"


#define STACK_LMIT 8*1024*1024
//allocate a new page in the page table
struct s_page* page_alloc(void* vaddr, bool writable){
  //printf("%x %x\n", vaddr,PHYS_BASE);
  //if(vaddr>PHYS_BASE) return NULL;
  struct thread* cur = thread_current();
  struct s_page* pg = malloc(sizeof(struct s_page));
  if(pg==NULL) return NULL;
  pg->vaddr = pg_round_down(vaddr);
  pg->writable = writable;
  pg->thd = cur;
  pg->frame = NULL;
  pg->file = NULL;
  pg->file_ofs = 0;
  pg->file_readsz = 0;
  pg->swapped = false;
  pg->sector_idx = (block_sector_t)-1;
  if(hash_insert(cur->pageTable,&pg->page_elem)!=NULL){
    free(pg);
    pg = NULL;
    //PANIC("alreay hashed");
  }

  return pg;
}

//unmapping the vaddr in the pagetable
void page_dealloc(struct s_page* pg){
  if(pg->frame!=NULL) {
    lock_acquire(&pg->frame->lock);
    frame_free(pg->frame);
    lock_release(&pg->frame->lock);
  }
  pg->frame = NULL;
  pagedir_clear_page(pg->thd->pagedir,pg->vaddr);
}


struct s_page* page_find(void* vaddr,void*esp){
  if(vaddr>=PHYS_BASE) return NULL;

  struct s_page tmp_page;
  struct thread* cur = thread_current();
  tmp_page.vaddr = (void*)pg_round_down(vaddr);
  //printf("%x\n",vaddr);

  struct hash_elem* e = hash_find(cur->pageTable,&tmp_page.page_elem);

  if(e!=NULL) {
    //printf("find entry\n");
    return hash_entry(e,struct s_page,page_elem);
  }
//TODO: stack growth
  //PANIC("hash size %d",hash_size(cur->pageTable));
  if(esp-32<vaddr&&vaddr>PHYS_BASE-STACK_LMIT){//&&thread_current()->stack - 32 < vaddr)
    //printf("sdadas %x %x\n",esp-32,vaddr);
    return page_alloc(tmp_page.vaddr,true);
  }
  return NULL;
}

bool page_map(struct s_page* page){
  //PANIC("page vaddr %x",page->vaddr);
  if(page==NULL) return false;
  if(page->frame==NULL){
    page->frame = frame_acquire(page);
    //ASSERT(!lock_held_by_current_thread(&page->frame->lock));
  //PANIC("entering page map");
  //TODO:swapping

    //ASSERT(page->frame!=NULL);
    //lock_acquire(&page->frame->lock);

    if(page->swapped) swap_in(page);
    else {
      if(page->file!=NULL){

      off_t read_bytes = file_read_at(page->file,page->frame->kpage,page->file_readsz,\
                    page->file_ofs);
      off_t zero_bytes = PGSIZE - read_bytes;
      memset(page->frame->kpage+read_bytes,0,zero_bytes);
      if(read_bytes!=page->file_readsz) return false;
    }else memset(page->frame->kpage,0,PGSIZE);
    }
  }
  //printf("%d %x %x\n",thread_current()->tid,page->vaddr,page->frame->kpage);
  bool success =
          pagedir_set_page(thread_current()->pagedir,page->vaddr,page->frame->kpage,page->writable);
  //lock_release(&page->frame->lock);


  return success;

}


//used for destroy_hash()
void page_destroy(struct hash_elem* e,void* aux UNUSED){
  struct s_page* pg=hash_entry(e,struct s_page,page_elem);
  if(pg->file!=NULL&&pagedir_is_dirty(pg->thd->pagedir,pg->vaddr)&&pg->frame!=NULL){
    lock_acquire(&pg->frame->lock);
     file_write_at(pg->file,pg->frame->kpage,pg->file_readsz,pg->file_ofs);
    lock_release(&pg->frame->lock);
   }
  //if(pg->file==NULL&&!pg->swapped) swap_out(pg);
  hash_delete(pg->thd->pageTable,e);
  if(pg!=NULL) {
    page_dealloc(pg);
    free(pg);
  }

}

//free the whole page table
void pageTable_free(){
  struct hash* spt = thread_current()->pageTable;
  if(spt!=NULL) hash_destroy(spt,page_destroy);
  free(spt);

}

//used for hash
bool page_less(const struct hash_elem* a,const struct hash_elem* b,void *aux UNUSED){
  struct s_page* page_a = hash_entry(a, struct s_page, page_elem);
  struct s_page* page_b = hash_entry(b, struct s_page, page_elem);
  return page_a->vaddr<page_b->vaddr;
}

unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct s_page *pg = hash_entry (e, struct s_page, page_elem);
  return ((uintptr_t) pg->vaddr) >> PGBITS;
}
