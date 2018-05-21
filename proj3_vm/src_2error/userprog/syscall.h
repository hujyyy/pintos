#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void syscall_exit(int status);
struct lock fs_lock;


#endif /* userprog/syscall.h */
