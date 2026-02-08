#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

 
#define SYS_YIELD   0
#define SYS_EXIT    1
#define SYS_PRINT   2
#define SYS_FORK    3
#define SYS_EXEC    4
#define SYS_WAIT    5
#define SYS_PIPE    6
#define SYS_READ    7
#define SYS_WRITE   8
#define SYS_CLOSE   9
#define SYS_DUP     10
#define SYS_OPEN    11
#define SYS_INIT_MODULE 12
#define SYS_DELETE_MODULE 13
#define SYS_GETPID      14
#define SYS_SLEEP       15
#define SYS_CHDIR       16
#define SYS_GETCWD      17
#define SYS_LSEEK       18
#define SYS_KILL        19
#define SYS_REBOOT      20

void syscall_init();

#endif
