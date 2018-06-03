#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry),true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e){
         //printf("ename: %s %s\n",name,e.name );
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
    }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;
  if(name==NULL) return false;
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  if(!inode_set_parentsector(dir->inode,inode_sector))
    goto done;
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  //printf("add %s %d\n",name,success);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  //printf("%d %s\n",inode_number(dir_get_inode(dir)),name );
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)){
    //printf("rm %s %d\n",name,inode_number(dir_get_inode(dir)));
    goto done;
  }
  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  if(inode_isdir(inode)&&(inode_get_opencnt(inode)>1||!dir_empty(inode))){
    //PANIC("%d\n",inode_get_opencnt(inode) );
    goto done;
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e){
    goto done;
  }
  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  //printf("remove %d\n",success );
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

char* get_filename(const char* pathname){
  if(pathname==NULL||strlen(pathname)==0) return NULL;
  char path[strlen(pathname)+1];
  memcpy(path,pathname,strlen(pathname)+1);
  char* last_token=NULL, *token=NULL,*remain=NULL;
  token = strtok_r(path,"/",&remain);
  if(token==NULL) return NULL;
  //ASSERT(token!=NULL);
  while(token!=NULL){
    last_token = token;
    token = strtok_r(NULL,"/",&remain);
  }
  //if(strcmp(last_token,".")==0||strcmp(last_token,"..")==0) return NULL;
  char* name = malloc(strlen(last_token)+1);
  memcpy(name,last_token,strlen(last_token)+1);
  return name;

}

struct dir* get_dir(char* pathname){
  if(pathname==NULL||strlen(pathname)==0) return NULL;
  char path[strlen(pathname)+1];
  memcpy(path,pathname,strlen(pathname)+1);
  struct thread* cur = thread_current();

  //PANIC("%d",cur->dir);

  struct dir* dir = NULL;
  char* head[1];
  memcpy(head,path,1);
  //printf("%s %d\n",path,memcmp(head,"/",1));
  if(memcmp(head,"/",1)==0||cur->dir==NULL) {
    dir = dir_open_root();
  }
  else dir = dir_reopen(cur->dir);

  //printf("get dir sector %d\n",inode_number(dir_get_inode(dir)) );
  char* last_token=NULL, *token=NULL,*remain=NULL;
  token = strtok_r(path,"/",&remain);

  //printf("token %s\n",token);

  bool meet_file = false;
  int success = -1;
  struct inode* inode;
  while(token!=NULL){
    last_token = token;
    if(strcmp(last_token,".")==0) {
      token=strtok_r(NULL,"/",&remain);
      continue;
    }
    else if (strcmp(last_token,"..")==0){
      block_sector_t tmp = inode_parentsector(dir->inode);
      inode = inode_open(tmp);
      if(!inode) return NULL;
    }else{
      success = dir_lookup(dir,last_token,&inode);
    }

    if(success){
      if(inode_isdir(inode)){//still visit a dir
        dir_close(dir);
        dir = dir_open(inode);
      }else {
        inode_close(inode);//reach the end of the pathname
        meet_file = true;
      }
    }
    token=strtok_r(NULL,"/",&remain);
  }
  //if the last entry is a dir and we've jumped in, should go back
  if(!meet_file&&success==1){
    struct dir* parent_dir = dir_open(dir_get_parentinode(dir));
    dir_close(dir);
    return parent_dir;
  }
  //printf("%s getdir dirsector %d\n",pathname, inode_number(dir_get_inode(dir)));
  //printf("end get %d\n",inode_get_opencnt(dir_get_inode(dir)));

  return dir;
}

struct inode* dir_get_parentinode(struct dir* dir){
  if(dir==NULL) return NULL;
  block_sector_t parent_sector = inode_parentsector(dir->inode);
  return inode_open(parent_sector);
}

bool dir_empty(struct inode* inode){
  struct dir_entry e;
  off_t pos = 0;

  while (inode_read_at (inode, &e, sizeof e, pos) == sizeof e)
    {
      pos += sizeof e;
      if (e.in_use)
        {
          return false;
        }
    }
  return true;

}
