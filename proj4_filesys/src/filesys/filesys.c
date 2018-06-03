#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size,bool is_dir)
{
  block_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();
  struct dir* dir = get_dir(name);
  char* filename = get_filename(name);
  //printf("%s %d %s\n",name,inode_number(dir_get_inode(dir)),filename);
  bool success = (dir != NULL && filename!=NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size,is_dir)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  //printf("success %d %d\n",success,inode_sector);
  if(filename!=NULL) free(filename);

  dir_close (dir);
  //printf("create %d\n",inode_get_opencnt(dir_get_inode(dir)));

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  //struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  struct dir* dir = get_dir(name);
  char* filename = get_filename(name);

  //printf("%s %d\n",filename,inode_number(dir_get_inode(dir)));
  // PANIC("%s",filename);

  if(dir!=NULL){
    if(filename==NULL){
      inode = dir_get_inode(dir);
    }
    else if(strcmp(filename,".")==0){
      inode = dir_get_inode(dir);
    }
    else if(strcmp(filename,"..")==0){
      inode = dir_get_parentinode(dir);
    }
    else{
      bool success = dir_lookup(dir, filename, &inode);
      //printf("filesysopen %s %d\n",name,success );
    }
  }
  // if(dir!=NULL)
  //   dir_lookup (dir, name, &inode);
  if(filename!=NULL)free(filename);
  dir_close (dir);
  if(inode==NULL) return NULL;
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  //struct dir *dir = dir_open_root ();
  //printf("%s\n",name );
  struct dir* dir = get_dir(name);
  char* filename = get_filename(name);

  //printf("filesys remove %d %d %s\n",inode_number(dir_get_inode(thread_current()->dir)),inode_number(dir_get_inode(dir)),name );

  if(filename==NULL) return false;
  //PANIC("%s",name);
  bool success = dir != NULL && dir_remove (dir, filename);
  free(filename);
  dir_close (dir);

  return success;
}

// bool filesys_chdir(const char *pathname){
//   struct dir* dir = get_dir(pathname);
//   char* dir_name = get_filename(pathname);
//   struct thread* cur = thread_current();
//
//   //printf("%s %x %s\n",pathname,dir,dir_name );
//   struct inode* inode = NULL;
//   if(dir!=NULL){
//     if(dir_name==NULL){
//       cur->dir = dir;
//       return true;
//     }else if(strcmp(dir_name,".")==0){
//       cur->dir = dir;
//       free(dir_name);
//       return true;
//     }else if(strcmp(dir_name,"..")==0){
//       inode = dir_get_parentinode(inode);
//     }else {
//       bool success = dir_lookup(dir,dir_name,&inode);
//     }
//
//     free(dir_name);
//     dir_close(dir);
//     if(inode!=NULL){
//       dir = dir_open(inode);
//     }else return false;
//
//     dir_close(cur->dir);
//     cur->dir = dir;
//     return true;
//   }
//   return false;
//
//
// }

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
