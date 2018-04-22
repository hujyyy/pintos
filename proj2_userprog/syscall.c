#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <user/syscall.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
//typedef void (*CALL_PROC) (struct intr_frame*);
void syscall_halt();
// void syscall_exit(int status);
int syscall_write (int fd,char* buffer,unsigned size);
int syscall_wait(tid_t tid);
pid_t syscall_exec(const char* cmd_line);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove(const char* file);
int syscall_open(const char* file);
int syscall_filesize(int fd);
int syscall_read (int fd, char* buffer, unsigned size);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);
//CALL_PROC call_functions[21];
void check_memory(void *ptr);



void
syscall_init (void)
{

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

}



static void
syscall_handler (struct intr_frame *f)
{



  int sys_num = *(int*)(f->esp);
  int32_t *ptr = f->esp;

  //if(sys_num>10)printf("%d\n", sys_num);
  switch (sys_num) {

    case SYS_HALT:{
      /* Halt the operating system.  */
      check_memory(ptr);
      syscall_halt();
    }

    case SYS_EXIT:{
      /* Terminate this process. */

      check_memory(ptr+1);
      int status = *((int *)ptr+1);
      if (status!=-1) f->eax = 0;
      syscall_exit(status);
    }

    case SYS_EXEC:{
      /* Start another process. */
      check_memory(ptr+1);
      char* cmd_line = (char*)*(ptr+1);
      pid_t pid = syscall_exec(cmd_line);
      f->eax = pid;
      return pid;
    }

    case SYS_WAIT:{
      /* Wait for a child process to die. */
      check_memory(ptr+1);

      tid_t tid=*((int*)f->esp+1);
      f->eax = syscall_wait(tid);
      return tid;
    }

    case SYS_CREATE:{
      /* Create a file. */
      check_memory(ptr+5);
      char* file = (char*)*(ptr+4);
      unsigned initial_size = *(unsigned*)(ptr+5);
      bool success = syscall_create(file,initial_size);
      //PANIC("%d",success);
      //printf("%s\n", file);
      f->eax = success;
      return success;
    }

    case SYS_REMOVE:{
      /* Delete a file. */
      check_memory(ptr+1);
      char* file = (char*)*(ptr+1);
      bool success = syscall_remove(file);
      f->eax = success;
      return success;
    }

    case SYS_OPEN:{
      /* Open a file. */
      check_memory(ptr+1);
      char* file = (char*)*(ptr+1);
      int fd = syscall_open(file);
      f->eax = fd;
      return fd;
    }

    case SYS_FILESIZE:{
      /* Obtain a file's size. */
      check_memory(ptr+1);
      int fd = *(ptr+1);
      int size = syscall_filesize(fd);
      f->eax = size;
      return size;
    }

    case SYS_READ:{
      /* Read from a file.  */
      check_memory(ptr+6);
      check_memory(*(ptr+6));
      //printf("%x\n", *(ptr+6));
      int fd = *(ptr+2);
      char *buffer = (char*)*(ptr+6);
      unsigned size = *(unsigned*)(ptr+3);
      int rd_size = syscall_read(fd,buffer,size);
      f->eax = rd_size;
      return rd_size;
    }

    case SYS_WRITE:{
      /* Write to a file. */
      check_memory(ptr+6);
      check_memory(*(ptr+6));
      //printf("%x\n", *(ptr+6));
      int fd = *(ptr+2);
      char *buffer = (char*)*(ptr+6);
      unsigned size = *(unsigned*)(ptr+3);
      int wt_size = syscall_write(fd,buffer,size);
      f->eax = wt_size;
      //printf("%d\n",wt_size );
      return wt_size;
    }

    case SYS_SEEK:{
      /* Change position in a file. */
      check_memory(ptr+5);
      int fd = *(int*)(ptr+4);
      unsigned position = *(unsigned*)(ptr+5);
      syscall_seek(fd,position);

    }

    case SYS_TELL:{
      /* Report current position in a file. */
      check_memory(ptr+1);
      int fd = *(ptr+1);
      unsigned result = syscall_tell(fd);
      f->eax = result;
    }

    case SYS_CLOSE:{
      /* Close a file. */
      check_memory(ptr+1);
      int fd = *(ptr+1);
      syscall_close(fd);
    }
    /* Project 3 and optionally project 4. */
    case SYS_MMAP:
      /* Map a file into memory. */

    case SYS_MUNMAP:
      /* Remove a memory mapping. */

    /* Project 4 only. */
    case SYS_CHDIR:
    /* Change the current directory. */

    case SYS_MKDIR:
    /* Create a directory. */

    case SYS_READDIR:
    /* Reads a directory entry. */

    case SYS_ISDIR:
    /* Tests if a fd represents a directory. */

    case SYS_INUMBER:
    /* Returns the inode number for a fd. */
    default:
    break;
  }
  //printf ("system call!\n");
  //thread_exit ();
}
// int check_range(struct intr_frame *f,int offset){
//     return (!is_user_vaddr(((int *)f->esp+offset)));
// }
void syscall_halt() {
  shutdown();
}

void syscall_exit(int status) {
  struct thread *current = thread_current();
  current-> exit_code = status;
  thread_exit();
}


pid_t syscall_exec(const char* cmd_line){
    if(cmd_line==NULL) {return -1;}

    char* file_name = (char*)malloc(sizeof(char*)*strlen(cmd_line)+1);
    memcpy(file_name,cmd_line,strlen(cmd_line)+1);

    tid_t tid = process_execute(file_name);
    //printf("%d\n", tid);
    struct thread* thd = getthread(tid);

    if(thd==NULL) {return -1;}
    //printf("a %d\n",thd->tid );
    struct thread* cur = thread_current();
    sema_down(&cur->execsema);
    //PANIC("%d",thd->tid);
    //printf("a %d\n",thd->tid );

    free(file_name);
    //printf("%d\n", thd->tid);
    return thd->tid;
}


int syscall_write(int fd, char* buffer,unsigned size ){

   //PANIC("%d xxx",fd);
   //printf("%d\n", size);
   if (size<=0) return 0;
   if (fd == 1) {
     if(size>512) return 0;
     putbuf(buffer,size);
     return size;
   }else{
     struct file* file_to_wt = getfile(fd);
     if(file_to_wt==NULL) return 0;

     return file_write(file_to_wt,buffer,size);
   }

}

int syscall_wait(tid_t tid){
    //printf("%d\n", tid);
    if(tid!=-1) return process_wait(tid);
    else return -1;

}

bool syscall_create (const char *file, unsigned initial_size){
  if(file==NULL||initial_size<0) syscall_exit(-1);
  else return filesys_create(file,initial_size);
}

bool syscall_remove(const char* file){
    return filesys_remove(file);
}

int syscall_open(const char* file){
  if(file==NULL) return -1;

  struct thread* cur = thread_current();
  struct file_info* fi = (struct file_info*)malloc(sizeof(struct file_info));
  fi->file = filesys_open(file);

  if(fi->file==NULL){
    free(fi);
    return -1;
  }else{
    fi->fd = ++(cur->maxfd);
    list_push_back(&cur->files_list,&fi->file_elem);
    return fi->fd;
  }
}

int syscall_filesize(int fd){
  if(fd<=1) return -1;

  struct file* file = getfile(fd);

  if(file==NULL) return -1;
  else return file_length(file);

}

int syscall_read(int fd,char* buffer, unsigned size){
  if(size<=0) return 0;

  if(fd == 0){
    int i;
    for(i=0;i<size;i++) buffer[i] = input_getc();
    return i;
  }else{
    struct file* file = getfile(fd);
    if(file==NULL) return -1;
    return file_read(file,buffer,size);
  }

}

void syscall_seek(int fd, unsigned postion){
  struct file* file = getfile(fd);
  file_seek(file,postion);
}

unsigned syscall_tell(int fd){
  if(fd<=1) return -1;
  struct file* file = getfile(fd);
  if(file==NULL) return -1;
  return file_tell(file);
}

void syscall_close(int fd){
  if(fd<=1) return;
  struct file_info* fi = getfileinfo(fd);
  if(fi!=NULL){
    file_close(fi->file);
    list_remove(&fi->file_elem);
    free(fi);
  }
}

void check_memory(void* ptr){
    //printf("%x %x\n", (int*)ptr+1,PHYS_BASE);
    if(is_kernel_vaddr(ptr)||\
    !check_page(thread_current()->pagedir,ptr)){
        //printf("djslajd %d\n", check_page(thread_current()->pagedir,ptr));
        syscall_exit(-1);
      }
}
