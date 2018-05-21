#include "threads/pte.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "page.h"



struct list_elem* last_e;
struct lock ftb_lock;

void frameTable_init(void){
  list_init(&frameTable);
  lock_init(&ftb_lock);
}

struct frame* frame_acquire(struct s_page* pg){

  void*  kpage;
  //if(pg->file==NULL) kpage = palloc_get_page(PAL_USER|PAL_ZERO);
  kpage = palloc_get_page(PAL_USER);
  struct frame* fr;
  if(kpage==NULL) {//no free frame available
    //PANIC("evicting...");
    fr = frame_evict();
  }else{
    fr = malloc(sizeof(struct frame));
    lock_init(&fr->lock);
    fr -> kpage = kpage;
    list_push_back(&frameTable,&fr->frame_elem);
  }

  //printf("%x\n", fr->kpage);
  ASSERT(!lock_held_by_current_thread(&fr->lock));
  lock_acquire(&fr->lock);
  fr -> thd = pg->thd;
  fr -> page = pg;
  lock_release(&fr->lock);


  return fr;
}

struct frame* frame_evict(){
  //printf("evicting\n" );
  //PANIC("evicting");
  //ASSERT(!lock_held_by_current_thread(&ftb_lock));
  lock_acquire(&ftb_lock);
  ASSERT(!list_empty(&frameTable));
  struct list_elem* e;
  struct frame* fr;

  //look for available frame

  for(e = list_begin(&frameTable);e!=list_end(&frameTable);e=list_next(e)){
    fr = list_entry(e,struct frame, frame_elem);
    if(!lock_try_acquire(&fr->lock)) continue;
    if(fr->page==NULL){
      ASSERT(fr->thd==NULL);
      lock_release(&fr->lock);
      lock_release(&ftb_lock);
      return fr;
    }
    lock_release(&fr->lock);
  }
  //PANIC("xxx");

  //if no available frame,look for victim
  if(last_e==NULL) last_e = list_begin(&frameTable);
  for(e = last_e;e!=list_end(&frameTable);e=list_next(e)){
    fr = list_entry(e,struct frame, frame_elem);
    //printf("id %d kpage %x\n",count,fr->kpage );
    if(!lock_try_acquire(&fr->lock)) continue;
    if(pagedir_is_accessed(fr->page->thd->pagedir,fr->page->vaddr)){
      pagedir_set_accessed(fr->page->thd->pagedir,fr->page->vaddr,false);
      lock_release(&fr->lock);
      continue;
    }else {
      //printf("kpage %x\n",fr->kpage );
      break;
    }
  }
  //PANIC("frametable size %d",count);
  //PANIC("xxxx");
  while(!lock_held_by_current_thread(&fr->lock)) {
    for(e = list_begin(&frameTable);e!=list_end(&frameTable);e=list_next(e)){
      fr = list_entry(e,struct frame, frame_elem);
      //printf("id %d kpage %x\n",count,fr->kpage );
      if(!lock_try_acquire(&fr->lock)) continue;
      if(pagedir_is_accessed(fr->page->thd->pagedir,fr->page->vaddr)){
        pagedir_set_accessed(fr->page->thd->pagedir,fr->page->vaddr,false);
        lock_release(&fr->lock);
        continue;
      }else {
        //printf("kpage %x\n",fr->kpage );
        break;
      }
    }
  }
  //if(e==list_end(&frameTable))PANIC("xxxx");
  //ASSERT(lock_held_by_current_thread(&fr->lock));
  lock_release(&ftb_lock);

  bool success = true;
  pagedir_clear_page(fr->page->thd->pagedir,(void*)fr->page->vaddr);
  lock_release(&fr->lock);

  if(fr->page->file==NULL) {
    //printf("swapping\n");
    success = swap_out(fr->page);
    //printf("xxxxx %d\n",success);
      //if(!success) PANIC("SWAPOUT FAILED");
  }


  else {
    if(pagedir_is_dirty(fr->page->thd->pagedir,fr->page->vaddr)){
      if(fr->page->writable) success=swap_out(fr->page);
      else {
        lock_acquire(&fr->lock);
        success=file_write_at(fr->page->file,(const void *) fr->kpage,fr->page->file_readsz,fr->page->file_ofs);
        lock_release(&fr->lock);

      }
    }
  }
  //pagedir_clear_page(fr->page->thd->pagedir,(void*)fr->page->vaddr);
  //lock_release(&fr->lock);
  if(success) {
    fr->page->frame = NULL;
    //frame_free(fr);
  }
  if(e!=list_end(&frameTable)) last_e = list_next(e);
  else last_e = list_begin(&frameTable);
  //page_dealloc(fr->page);
  //frame_free(fr);
  return fr;


}





void frame_free(struct frame* fr){
    fr->thd = NULL;
    fr->page = NULL;
}

void frameTable_free(void){
  struct list_elem* f_elem;

  while(!list_empty(&frameTable)){
    f_elem = list_pop_front(&frameTable);
    struct frame* fr = list_entry(f_elem,struct frame,frame_elem);
    free(fr);
  }

}
