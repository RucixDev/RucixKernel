#include "pipe.h"
#include "heap.h"
#include "process.h"
#include "string.h"
#include "console.h"
#include "vfs.h"

 
int pipe_read(struct file *file, char *buffer, int size, uint64_t *offset);
int pipe_write(struct file *file, const char *buffer, int size, uint64_t *offset);
int pipe_close(struct inode *inode, struct file *file);

static struct file_operations pipe_ops = {
    .read = pipe_read,
    .write = pipe_write,
    .release = pipe_close
};

extern struct process* current_process;

int pipe_create(int fds[2]) {
    pipe_t* p = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!p) return -1;
    
     
    char* ptr = (char*)p;
    for(uint64_t i=0; i<sizeof(pipe_t); i++) ptr[i] = 0;
    
    spinlock_init(&p->lock);
    wait_queue_init(&p->read_wait);
    wait_queue_init(&p->write_wait);
    p->readers = 1;
    p->writers = 1;
    
    struct file* r_file = (struct file*)kmalloc(sizeof(struct file));
    struct file* w_file = (struct file*)kmalloc(sizeof(struct file));
    
    if (!r_file || !w_file) {
        if (r_file) kfree(r_file);
        if (w_file) kfree(w_file);
        kfree(p);
        return -1;
    }
    
    memset(r_file, 0, sizeof(struct file));
    r_file->f_count = 1;
    r_file->f_flags = O_RDONLY;
    r_file->f_mode = O_RDONLY;
    r_file->f_op = &pipe_ops;
    r_file->private_data = p;
    r_file->f_dentry = 0;  
    
    memset(w_file, 0, sizeof(struct file));
    w_file->f_count = 1;
    w_file->f_flags = O_WRONLY;
    w_file->f_mode = O_WRONLY;
    w_file->f_op = &pipe_ops;
    w_file->private_data = p;
    w_file->f_dentry = 0;  
    
    int fd0 = -1, fd1 = -1;
    for(int i=0; i<MAX_FILES; i++) {
        if (!current_process->fd_table[i]) {
            if (fd0 == -1) fd0 = i;
            else {
                fd1 = i;
                break;
            }
        }
    }
    
    if (fd0 == -1 || fd1 == -1) {
         
        return -1;
    }
    
    current_process->fd_table[fd0] = r_file;
    current_process->fd_table[fd1] = w_file;
    
    fds[0] = fd0;
    fds[1] = fd1;
    
    return 0;
}

int pipe_read(struct file *file, char *buffer, int size, uint64_t *offset) {
    (void)offset;  
    if (file->f_flags == O_WRONLY) return -1;
    
    pipe_t* p = (pipe_t*)file->private_data;
    int bytes_read = 0;
    
    while (bytes_read < size) {
        spinlock_acquire(&p->lock);
        
        while (p->bytes_available == 0) {
            if (p->writers == 0) {
                spinlock_release(&p->lock);
                return bytes_read;
            }
            
            spinlock_release(&p->lock);
            sleep_on(&p->read_wait);
            spinlock_acquire(&p->lock);
        }
        
        buffer[bytes_read++] = p->buffer[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_SIZE;
        p->bytes_available--;
        
        wake_up(&p->write_wait);
        
        spinlock_release(&p->lock);
    }
    
    return bytes_read;
}

int pipe_write(struct file *file, const char *buffer, int size, uint64_t *offset) {
    (void)offset;
    if (file->f_flags == O_RDONLY) return -1;
    
    pipe_t* p = (pipe_t*)file->private_data;
    int bytes_written = 0;
    
    while (bytes_written < size) {
        spinlock_acquire(&p->lock);
        
        while (p->bytes_available == PIPE_SIZE) {
            if (p->readers == 0) {
                spinlock_release(&p->lock);
                return -1;  
            }
            
            spinlock_release(&p->lock);
            sleep_on(&p->write_wait);
            spinlock_acquire(&p->lock);
        }
        
        p->buffer[p->write_pos] = buffer[bytes_written++];
        p->write_pos = (p->write_pos + 1) % PIPE_SIZE;
        p->bytes_available++;
        
        wake_up(&p->read_wait);
        
        spinlock_release(&p->lock);
    }
    
    return bytes_written;
}

int pipe_close(struct inode *inode, struct file *file) {
    (void)inode;
    pipe_t* p = (pipe_t*)file->private_data;
    
    spinlock_acquire(&p->lock);
    
    if (file->f_flags == O_RDONLY) {
        p->readers--;
        if (p->readers == 0) {
            wake_up(&p->write_wait);
        }
    } else {
        p->writers--;
        if (p->writers == 0) {
            wake_up(&p->read_wait);
        }
    }
    
    int free_pipe = (p->readers == 0 && p->writers == 0);
    spinlock_release(&p->lock);
    
    if (free_pipe) {
        kfree(p);
    }
    
     
    return 0;
}
