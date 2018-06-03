#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INODE_BLOCK_SIZE 14
#define INDIRECT_BLOCK_IDX 12
#define DOUBLE_INDIRECT_BLOCK_IDX 13
#define INDIRE_SECTORS_NUM 128
#define DOUBLE_INDIRECT_SECTORS_NUM 128*128
#define FILE_SECTORS_LIMIT 12+128+128*128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */

    struct lock lock;
    uint32_t blocks[INODE_BLOCK_SIZE]; //0-11 direct blocks
                                      //12   indirect block
                                      //13   double-indirect block

    uint32_t unused[125-INODE_BLOCK_SIZE-6];     /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL&&pos>=0);
  ASSERT (pos<=(12+INDIRE_SECTORS_NUM+DOUBLE_INDIRECT_SECTORS_NUM)*BLOCK_SECTOR_SIZE);
  // if (pos < inode->data.length)
  //   return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  // else
  //   return -1;
  //printf("byte to sector %d\n", inode->data.blocks[pos/BLOCK_SECTOR_SIZE]);

  if(pos<BLOCK_SECTOR_SIZE*12) {
    //printf("%d\n", pos/BLOCK_SECTOR_SIZE);
    block_sector_t ret = inode->data.blocks[pos/BLOCK_SECTOR_SIZE];
    return ret;
  }
  else if(pos-BLOCK_SECTOR_SIZE*12<BLOCK_SECTOR_SIZE*INDIRE_SECTORS_NUM){
    block_sector_t tmp[INDIRE_SECTORS_NUM];
    block_read(fs_device,inode->data.blocks[INDIRECT_BLOCK_IDX],tmp);
    block_sector_t ret = tmp[(pos-BLOCK_SECTOR_SIZE*12)/BLOCK_SECTOR_SIZE];
    return ret;
  }
  else{
    int rest_pos = pos - (12+INDIRE_SECTORS_NUM)*BLOCK_SECTOR_SIZE;
    block_sector_t tmp[INDIRE_SECTORS_NUM];
    block_read(fs_device,inode->data.blocks[DOUBLE_INDIRECT_BLOCK_IDX],tmp);
    block_sector_t lv1_idx = rest_pos/(INDIRE_SECTORS_NUM*BLOCK_SECTOR_SIZE);
    block_read(fs_device,tmp[lv1_idx],tmp);
    block_sector_t lv2_idx = (rest_pos%(INDIRE_SECTORS_NUM*BLOCK_SECTOR_SIZE))/BLOCK_SECTOR_SIZE;
    return tmp[lv2_idx];
  }

}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  //printf("length %d",length);
  struct inode_disk *disk_inode = NULL;
  static char zeros[BLOCK_SECTOR_SIZE];

  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);


  if(disk_inode != NULL){
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      lock_init(&disk_inode->lock);
      size_t sectors = bytes_to_sectors(length);
      //printf("sectors %d\n",sectors );
      int direct_blocks_num = sectors>12?12:sectors;
      //direct blocks set up
      for(int i=0;i<direct_blocks_num;i++){
        if(free_map_allocate(1,&disk_inode->blocks[i])){
            block_write(fs_device,disk_inode->blocks[i],zeros);
        }else {
          free(disk_inode);
          return false;
        }
      }
      success = true;
      //printf("direct_blocks_num %d\n",direct_blocks_num);
      int rest_blocks_num = sectors - 12;

      //indirect blocks set up
      if(rest_blocks_num>0){
          int indirect_blocks_num = rest_blocks_num>INDIRE_SECTORS_NUM?INDIRE_SECTORS_NUM:rest_blocks_num;
          success=indirect_blocks_alloc(indirect_blocks_num,&disk_inode->blocks[INDIRECT_BLOCK_IDX],1);
          //PANIC("%d\n",disk_inode->blocks[12]);
          rest_blocks_num -= INDIRE_SECTORS_NUM;
          if(!success) {
            free(disk_inode);
            return false;
          }
          //double indirect blocks set up

          if(rest_blocks_num>0){
            //printf("rest_blocks_num %d\n", rest_blocks_num );
            ASSERT(rest_blocks_num<=DOUBLE_INDIRECT_SECTORS_NUM);

            if(rest_blocks_num>DOUBLE_INDIRECT_SECTORS_NUM){
              free(disk_inode);
              return false;
            }
            success = indirect_blocks_alloc(rest_blocks_num,&disk_inode->blocks[DOUBLE_INDIRECT_BLOCK_IDX],2);
          }

      }

    block_write(fs_device,sector,disk_inode);
    //printf("qqqqq %d\n",bytes_to_sectors(length));
    free(disk_inode);
  }

  // if (disk_inode != NULL)
  //   {
  //     size_t sectors = bytes_to_sectors (length);
  //     disk_inode->length = length;
  //     disk_inode->magic = INODE_MAGIC;
  //     if (free_map_allocate (sectors, &disk_inode->start))
  //       {
  //         block_write (fs_device, sector, disk_inode);
  //         if (sectors > 0)
  //           {
  //             static char zeros[BLOCK_SECTOR_SIZE];
  //             size_t i;
  //
  //             for (i = 0; i < sectors; i++){
  //               block_write (fs_device, disk_inode->start + i, zeros);
  //             }
  //           }
  //         success = true;
  //       }
  //     free (disk_inode);
  //   }

  return success;
}


bool indirect_blocks_alloc(int sectors_num,block_sector_t *sectorp,int lev){
  static char zeros[BLOCK_SECTOR_SIZE];
  ASSERT(lev==1||lev==2);
  bool success;
  success = free_map_allocate(1,sectorp);
  //if(lev==2) PANIC("%d",success);
  if(success){
    block_write(fs_device,*sectorp,zeros);
    block_sector_t tmp[INDIRE_SECTORS_NUM];

    if(lev==1){
      //ASSERT(sectors_num>0&&sectors_num<=BLOCK_SECTOR_SIZE);
      for(int i=0;i<sectors_num;i++){
        success = free_map_allocate(1,&tmp[i]);
        block_write(fs_device,tmp[i],zeros);
      }
    }
    else{
      //PANIC("XXXX");
      ASSERT(sectors_num>0&&sectors_num<=DOUBLE_INDIRECT_SECTORS_NUM);
        int sectors_left = sectors_num;
        int count = 0;
        while(sectors_left>0){
          int sectors_to_alloc = sectors_left>INDIRE_SECTORS_NUM?INDIRE_SECTORS_NUM:sectors_left;
          success = indirect_blocks_alloc(sectors_to_alloc,&tmp[count++],1);
          if(!success) break;
          sectors_left -= sectors_to_alloc;
        }
    }
    //ASSERT(success);
    if(success) block_write(fs_device,*sectorp,tmp);
    else return false;
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  //block_read (fs_device, inode->sector, &inode->data);
  // struct cache_entry* entry = cache_get(inode->sector);
  // memcpy(&inode->data,entry->buffer,BLOCK_SECTOR_SIZE);
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  //printf("open_cnt %d\n",inode->open_cnt );
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          //PANIC("XXX");
          inode_release(inode);
          free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length));
        }

      free (inode);
    }
}

void inode_release(struct inode* inode){
  struct inode_disk* inode_disk = &inode->data;
  size_t sectors = bytes_to_sectors(inode_disk->length);
  int direct_num = sectors<12?sectors:12;
  for(int i=0;i<direct_num;i++){
    free_map_release(inode_disk->blocks[i],1);
  }
  int rest_sectors = sectors - 12;
  if(rest_sectors>0){
    int indirect_num = rest_sectors>INDIRE_SECTORS_NUM?INDIRE_SECTORS_NUM:rest_sectors;
    indirect_release(indirect_num,inode_disk->blocks[INDIRECT_BLOCK_IDX],1);

    rest_sectors -= INDIRE_SECTORS_NUM;
    if(rest_sectors>0)
      indirect_release(rest_sectors,inode_disk->blocks[DOUBLE_INDIRECT_BLOCK_IDX],2);
  }

}

void indirect_release(int sectors_num,block_sector_t sector,int lev){
  block_sector_t tmp[INDIRE_SECTORS_NUM];
  block_read(fs_device,sector,tmp);
  if(lev==1){
    for(int i=0;i<sectors_num;i++)
      free_map_release(tmp[i],1);
  }else{
    int rest_sectors = sectors_num;
    int count = 0;
    while(rest_sectors>0){
      int sectors_to_release = rest_sectors>INDIRE_SECTORS_NUM?INDIRE_SECTORS_NUM:rest_sectors;
      indirect_release(sectors_to_release,tmp[count++],1);
      rest_sectors -= sectors_to_release;
    }
  }
  free_map_release(sector,1);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  //uint8_t *bounce = NULL;

  //struct cache_entry* ent = malloc (sizeof(struct cache_entry));
  //int count = 0;
  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      //if(chunk_size==0)printf("length %d offset %d size %d \n", inode_length (inode),offset,size);
      //printf("sector_id %d sectors %d\n",bytes_to_sectors(offset),bytes_to_sectors(inode_length (inode)));
      if (chunk_size <= 0)
        break;

      //printf("offset %d size %d\n",offset,size );
      //lock_acquire(&inode->data.lock);
      struct cache_entry* entry = cache_get(sector_idx);
      //lock_release(&inode->data.lock);

      memcpy(buffer+bytes_read,entry->buffer+sector_ofs,chunk_size);

        //memcpy(buffer+bytes_read,tmp+sector_ofs,chunk_size);


      //if(memcmp(entry->buffer,tmp,BLOCK_SECTOR_SIZE)!=0) printf("%d %d\n",tmp[0],entry->buffer[0] );
      //printf("sector idx %d ofs %d chunksize %d\n\n", sector_idx,offset,chunk_size);
      //
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //
      //     block_read (fs_device, sector_idx, buffer + bytes_read);
      //
      //   }
      // else
      //   {
      //
      //     /* Read sector into bounce buffer, then partially copy
      //        into caller's buffer. */
      //     if (bounce == NULL)
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //     block_read (fs_device, sector_idx, bounce);
      //     //cache_read(sector_idx,buffer+bytes_read);
      //     memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      //     //free(tmp);
      //   }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  size_t length = inode_length(inode);
  //printf("write size %d offset %d length %d\n", size, offset,inode_length(inode));

  if(size+offset>length){
    //lock_acquire(&inode->data.lock);
    inode_growth(inode,bytes_to_sectors(size+offset)-bytes_to_sectors(length));
    inode->data.length = size+offset;
    block_write(fs_device,inode->sector,&inode->data);
    //printf("write size %d offset %d length %d\n", size, offset,inode_length(inode));
    //lock_release(&inode->data.lock);
  }
  //PANIC("size %d",size);
  //ASSERT(size+offset<=inode_length(inode));
  //printf("size %d offset %d length %d\n", size, offset,inode_length(inode));
  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      //PANIC("sector_idx %d  sector_ofs %d",sector_idx,sector_ofs);
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      //PANIC("inode_left %d chunksize %d",inode_left,chunk_size);
      if (chunk_size <= 0)
        break;

      // printf("sector idx  %d\n",sector_idx );
      //lock_acquire(&inode->data.lock);
      struct cache_entry* entry = cache_get(sector_idx);
      //lock_release(&inode->data.lock);

      memcpy(entry->buffer+sector_ofs,buffer+bytes_written,chunk_size);
      entry->dirty = true;
      entry->accessed = true;
      // printf("sdjalkdjklsjadlk\n");
      //block_write (fs_device, sector_idx, entry->buffer);
      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Write full sector directly to disk. */
      //     block_write (fs_device, sector_idx, buffer + bytes_written);
      //   }
      // else
      //   {
      //     /* We need a bounce buffer. */
      //     if (bounce == NULL)
      //       {
      //         bounce = malloc (BLOCK_SECTOR_SIZE);
      //         if (bounce == NULL)
      //           break;
      //       }
      //
      //     /* If the sector contains data before or after the chunk
      //        we're writing, then we need to read in the sector
      //        first.  Otherwise we start with a sector of all zeros. */
      //     if (sector_ofs > 0 || chunk_size < sector_left){
      //       block_read (fs_device, sector_idx, bounce);
      //       //cache_read(sector_idx,bounce);
      //     }
      //     else
      //       memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //     memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //     block_write (fs_device, sector_idx, bounce);
      //   }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  //free (bounce);
  //PANIC("bytes_written %d",bytes_written);
  return bytes_written;
}

void inode_growth(struct inode* inode, int growth_size){
  static char zeros[BLOCK_SECTOR_SIZE];
  struct inode_disk* inode_disk = &inode->data;
  size_t sectors = bytes_to_sectors(inode_disk->length);
  int rest_to_grow = growth_size;

  //printf("growth size %d sectors %d\n",growth_size,sectors );

  ASSERT(sectors+growth_size<=FILE_SECTORS_LIMIT);

  int direct_start = -1;
  int direct_to_grow = 0;

  int indirect_start = -1;
  int indirect_to_grow = 0;

  int double_indirect_start1 = -1;
  int double_indirect_start2 = -1;
  int double_indirect_to_grow = 0;

  //direct blocks not full
  if(sectors<12)  {
    direct_start = sectors;
    direct_to_grow = 12-sectors>=rest_to_grow?rest_to_grow:12-sectors;
    //printf("direct_start %d  direct_to_grow %d\n",direct_start,direct_to_grow );

    for(int i = direct_start;i<direct_start+direct_to_grow;i++){
      if(free_map_allocate(1,&inode_disk->blocks[i])){
          block_write(fs_device,inode_disk->blocks[i],zeros);
        }
    }

    rest_to_grow -= direct_to_grow;
  }
  //printf("%d %d\n",rest_to_grow, growth_size);

  //indirect blocks not full
  if(rest_to_grow>0&&sectors<12+INDIRE_SECTORS_NUM){
    indirect_start = sectors>12?sectors-12:0;
    indirect_to_grow = INDIRE_SECTORS_NUM-indirect_start>=rest_to_grow\
                        ?rest_to_grow:INDIRE_SECTORS_NUM-indirect_start;
    inode_alloc_growth(indirect_start,0,indirect_to_grow,&inode_disk->blocks[INDIRECT_BLOCK_IDX],1);
    rest_to_grow -= indirect_to_grow;
    //printf("%d %d\n",indirect_start, indirect_to_grow);

  }
  //printf("%d %d\n",rest_to_grow, growth_size);
  //double indirect blocks not full
  if(rest_to_grow>0){
    int temp = sectors>12+INDIRE_SECTORS_NUM?sectors-12-INDIRE_SECTORS_NUM:0;
    double_indirect_start1 = temp/INDIRE_SECTORS_NUM;
    double_indirect_start2 = temp%INDIRE_SECTORS_NUM;
    double_indirect_to_grow = rest_to_grow;
    inode_alloc_growth(double_indirect_start1,double_indirect_start2,\
      double_indirect_to_grow,&inode_disk->blocks[DOUBLE_INDIRECT_BLOCK_IDX],2);

  }

}

void inode_alloc_growth(int start1,int start2,int sectors_num,block_sector_t* sectorp,int lev){
  static char zeros[BLOCK_SECTOR_SIZE];
  ASSERT(lev==1||lev==2);
  block_sector_t tmp[INDIRE_SECTORS_NUM];
  if(start1==0) {
    free_map_allocate(1,sectorp);
    block_write(fs_device,*sectorp,zeros);
  }
  else block_read(fs_device,*sectorp,tmp);

  if(lev==1){
    ASSERT(sectors_num>0&&sectors_num<=DOUBLE_INDIRECT_SECTORS_NUM-start1);
    for(int i=start1;i<start1+sectors_num;i++){
      if(free_map_allocate(1,&tmp[i]))
        block_write(fs_device,tmp[i],zeros);
    }
  }
  else{
    ASSERT(sectors_num>0&&sectors_num<=DOUBLE_INDIRECT_SECTORS_NUM);
      int sectors_left = sectors_num;
      int count = start1;
      int start = start2;
      while(sectors_left>0){
        int sectors_to_alloc = sectors_left>INDIRE_SECTORS_NUM-start2?INDIRE_SECTORS_NUM-start2:sectors_left;
        inode_alloc_growth(start,0,sectors_to_alloc,&tmp[count++],1);
        sectors_left -= sectors_to_alloc;
        start = 0;
      }
  }
  //ASSERT(success);
  block_write(fs_device,*sectorp,tmp);


}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
