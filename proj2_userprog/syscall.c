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
void syscall_exit(struct intr_frame *);
int syscall_write (struct intr_frame *);
int syscall_wait(struct intr_frame *);
//int check_range (struct intr_frame*,int space);

//CALL_PROC call_functions[21];

void
syscall_init (void)
{

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

}



static void
syscall_handler (struct intr_frame *f UNUSED)
{

  int sys_num = *(int*)(f->esp);
  int32_t *ptr = f->esp;
  // if(lock_state==0){
  //   lock_init(&file_system_lock);
  //   lock_state = 1;
  // }
  switch (sys_num) {

    case SYS_HALT:


    /* Halt the operating system.  */
      syscall_halt();
      break;
    case SYS_EXIT:
    /* Terminate this process. */
      syscall_exit(f);

    case SYS_EXEC:
    /* Start another process. */

    case SYS_WAIT:
    /* Wait for a child process to die. */
    syscall_wait(f);

    case SYS_CREATE:
    /* Create a file. */

    case SYS_REMOVE:
    /* Delete a file. */

    case SYS_OPEN:
    /* Open a file. */

    case SYS_FILESIZE:
    /* Obtain a file's size. */

    case SYS_READ:
    /* Read from a file.  */

    case SYS_WRITE:
    /* Write to a file. */
      syscall_write(f);

    case SYS_SEEK:
    /* Change position in a file. */

    case SYS_TELL:
    /* Report current position in a file. */

    case SYS_CLOSE:
    /* Close a file. */

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
  printf ("system call!\n");
  //thread_exit ();
}
// int check_range(struct intr_frame *f,int offset){
//     return (!is_user_vaddr(((int *)f->esp+offset)));
// }
void syscall_halt() {
  shutdown();
}
void syscall_exit(struct intr_frame *f) {
  int32_t *ptr = f->esp;
  struct thread *current = thread_current();
  int status = *((int *)ptr+1);
  current-> exit_code = status;
  if (status!=-1) f->eax = 0;
  thread_exit();
}


int syscall_write(struct intr_frame* f){
  int32_t *ptr = f->esp;
  int fd = *(ptr+2);
  char *buffer = (char*)*(ptr+6);
  unsigned size = *(unsigned*)(ptr+3);
   //PANIC("%d xxx",fd);
   printf("%d\n", size);
   if (size<=0) f->eax = size;
   if (fd == 1) {
     putbuf(buffer,size);
     f->eax = size;
   }

   // lock_acquire(&file_system_lock);

}

int syscall_wait(struct intr_frame *f){
    tid_t tid=*((int*)f->esp+1);
    if(tid!=-1) f->eax = process_wait(tid);
    else f->eax = -1;

}
//
// void syscall_create(struct intr_frame *f) {
//
// }
//
// void syscall_filesize(struct intr_frame *f) {
//    if(check_range(f,2)) syscall_errorexit(-1);
//    struct thread *current = thread_current();
//    int arg0 = *((int *)f->esp+1);
//    //struct file_node *fn
// }
