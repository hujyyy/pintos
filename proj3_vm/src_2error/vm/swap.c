#include <stdio.h>
#include "vm/page.h"
#include "vm/swap.h"
#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "threads/synch.h"

struct bitmap* swap_bitmap;
struct lock swap_lock;

#define SECTORS_PER_PAGE (PGSIZE/BLOCK_SECTOR_SIZE)



void swap_init(){

  swapBlock = block_get_role(BLOCK_SWAP);

  if(swapBlock!=NULL)swap_bitmap = bitmap_create(block_size(swapBlock)/SECTORS_PER_PAGE);
  //printf("bit map size %d should be %d\n",bitmap_size(swap_bitmap),block_size(swapBlock)/SECTORS_PER_PAGE);
  lock_init(&swap_lock);
  //PANIC("block_size(swapBlock): %d",block_size(swapBlock));
}

void swap_in(struct s_page* pg){
  //printf("tid %d ", thread_current()->tid);
  //printf("SWAPIN  vaddr %x kpage %x, sector %d\n",pg->vaddr,pg->frame->kpage,pg->sector_idx);
  //ASSERT(pg->sector_idx!=(block_sector_t)-1);
  for(int i=0;i<SECTORS_PER_PAGE;i++){
    block_read(swapBlock,pg->sector_idx+i,pg->frame->kpage+i*BLOCK_SECTOR_SIZE);
  }

  //printf("%d %d reset idx %d\n",pg->sector_idx,SECTORS_PER_PAGE, ((int)pg->sector_idx)/(int)SECTORS_PER_PAGE );
  lock_acquire(&swap_lock);
  bitmap_reset(swap_bitmap,pg->sector_idx/SECTORS_PER_PAGE);
  lock_release(&swap_lock);
  pg->sector_idx = (block_sector_t)-1;
  pg->swapped = false;
}


bool swap_out(struct s_page* pg){
    //ASSERT(!lock_held_by_current_thread(&swap_lock));
    lock_acquire(&swap_lock);
    size_t bitmap_idx = bitmap_scan_and_flip(swap_bitmap,0,1,false);
    lock_release(&swap_lock);

    //printf("bit map size%d\n",bitmap_size(swap_bitmap));
    if(bitmap_idx==BITMAP_ERROR) PANIC("bit map error");

    pg->sector_idx = (block_sector_t)bitmap_idx*SECTORS_PER_PAGE;

    //printf("slot: %d\n", bitmap_idx);
    //printf("tid %d ", thread_current()->tid);
    //printf("SWAPOUT vaddr %x kpage %x\n",pg->vaddr,(uint8_t *)pg->frame->kpage );

    for(int i=0;i<SECTORS_PER_PAGE;i++){
      //printf("%x\n",(uint8_t *)pg->frame->kpage+i*BLOCK_SECTOR_SIZE );
      block_write(swapBlock,pg->sector_idx+i,(uint8_t *)pg->frame->kpage+i*BLOCK_SECTOR_SIZE);

    }

    pg->swapped = true;
    pg->file = NULL;
    pg->file_readsz = 0;
    pg->file_ofs = 0;

    return true;
}

void swap_free(){
  free(swapBlock);
  free(swap_bitmap);
}
