#include "vfs.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "process.h"  
#include "list.h"

static struct file_system_type *file_systems = 0;
static LIST_HEAD(super_blocks);
static spinlock_t file_systems_lock;
static spinlock_t super_blocks_lock;

#define DENTRY_HASH_SIZE 1024
static struct list_head dentry_hashtable[DENTRY_HASH_SIZE];
static spinlock_t dcache_lock;

struct vfsmount *root_mnt = 0;
struct dentry *root_dentry = 0;

static LIST_HEAD(vfsmount_list);
static spinlock_t vfsmount_lock;

static struct vfsmount *lookup_mnt(struct vfsmount *mnt, struct dentry *dentry) {
    spinlock_acquire(&vfsmount_lock);
    struct list_head *pos;
    list_for_each(pos, &vfsmount_list) {
        struct vfsmount *m = list_entry(pos, struct vfsmount, mnt_hash);
        if (m->mnt_parent == mnt && m->mnt_mountpoint == dentry) {
             spinlock_release(&vfsmount_lock);
             return m;
        }
    }
    spinlock_release(&vfsmount_lock);
    return 0;
}

static void follow_mount(struct vfsmount **mnt, struct dentry **dentry) {
    while (1) {
        struct vfsmount *mounted = lookup_mnt(*mnt, *dentry);
        if (!mounted) break;
        *mnt = mounted;
        *dentry = mounted->mnt_root;
    }
}

static unsigned int d_hash(struct dentry *parent, const char *name) {
    unsigned int hash = 0;
    unsigned int len = strlen(name);
    for (unsigned int i = 0; i < len; i++) {
        hash = (hash << 5) - hash + name[i];
    }
    hash ^= (unsigned long)parent;
    return hash % DENTRY_HASH_SIZE;
}

struct dentry *alloc_dentry(struct dentry *parent, const char *name) {
    struct dentry *dentry = (struct dentry *)kmalloc(sizeof(struct dentry));
    if (!dentry) return 0;
    
    memset(dentry, 0, sizeof(struct dentry));
    dentry->d_count = 1;
    dentry->d_parent = parent;
    
    int len = strlen(name);
    char *name_copy = (char *)kmalloc(len + 1);
    if (!name_copy) {
        kfree(dentry);
        return 0;
    }
    memcpy(name_copy, name, len);
    name_copy[len] = 0;
    
    dentry->d_name.name = name_copy;
    dentry->d_name.len = len;
    dentry->d_name.hash = d_hash(parent, name_copy);
    
    INIT_LIST_HEAD(&dentry->d_hash);
    INIT_LIST_HEAD(&dentry->d_lru);
    INIT_LIST_HEAD(&dentry->d_subdirs);
    INIT_LIST_HEAD(&dentry->d_child);
    INIT_LIST_HEAD(&dentry->d_alias);
    spinlock_init(&dentry->d_lock);
    
    if (parent) {
        dentry->d_sb = parent->d_sb;
        spinlock_acquire(&parent->d_lock);
        list_add(&dentry->d_child, &parent->d_subdirs);
        spinlock_release(&parent->d_lock);
    }
    
    return dentry;
}

void d_add(struct dentry *entry, struct inode *inode) {
    if (inode) {
        spinlock_acquire(&inode->i_lock);
        list_add(&entry->d_alias, &inode->i_dentry);
        spinlock_release(&inode->i_lock);
        entry->d_inode = inode;
    }
    
    spinlock_acquire(&dcache_lock);
    list_add(&entry->d_hash, &dentry_hashtable[entry->d_name.hash]);
    spinlock_release(&dcache_lock);
}

void d_instantiate(struct dentry *entry, struct inode *inode) {
    if (!entry) return;
    if (inode) {
        spinlock_acquire(&inode->i_lock);
        list_add(&entry->d_alias, &inode->i_dentry);
        spinlock_release(&inode->i_lock);
        entry->d_inode = inode;
    }
}

struct dentry *d_lookup(struct dentry *parent, const char *name) {
    unsigned int hash = d_hash(parent, name);
    
    spinlock_acquire(&dcache_lock);
    struct list_head *pos;
    list_for_each(pos, &dentry_hashtable[hash]) {
        struct dentry *dentry = list_entry(pos, struct dentry, d_hash);
        if (dentry->d_parent == parent && strcmp(dentry->d_name.name, name) == 0) {
            dentry->d_count++;
            spinlock_release(&dcache_lock);
            return dentry;
        }
    }
    spinlock_release(&dcache_lock);
    return 0;
}

 

struct inode *new_inode(struct super_block *sb) {
    struct inode *inode = (struct inode *)kmalloc(sizeof(struct inode));
    if (!inode) return 0;
    
    memset(inode, 0, sizeof(struct inode));
    inode->i_sb = sb;
    inode->i_count = 1;
    spinlock_init(&inode->i_lock);
    INIT_LIST_HEAD(&inode->i_dentry);
    
    if (sb) {
        spinlock_acquire(&sb->s_lock);
        list_add(&inode->i_sb_list, &sb->s_inodes);
        spinlock_release(&sb->s_lock);
    }
    
    return inode;
}

int register_filesystem(struct file_system_type *fs) {
    spinlock_acquire(&file_systems_lock);
    struct file_system_type **p = &file_systems;
    while (*p) {
        if (strcmp((*p)->name, fs->name) == 0) {
            spinlock_release(&file_systems_lock);
            return -1;  
        }
        p = &(*p)->next;
    }
    fs->next = 0;
    *p = fs;
    spinlock_release(&file_systems_lock);
    return 0;
}

struct file_system_type *get_fs_type(const char *name) {
    spinlock_acquire(&file_systems_lock);
    struct file_system_type *fs = file_systems;
    while (fs) {
        if (strcmp(fs->name, name) == 0) {
            spinlock_release(&file_systems_lock);
            return fs;
        }
        fs = fs->next;
    }
    spinlock_release(&file_systems_lock);
    return 0;
}

struct super_block *vfs_mount(const char *fs_type, int flags, const char *dev_name, void *data) {
    struct file_system_type *fs = get_fs_type(fs_type);
    if (!fs) return 0;
    
    struct super_block *sb = fs->mount(fs, flags, dev_name, data);
    if (!sb) return 0;

    spinlock_acquire(&super_blocks_lock);
    list_add(&sb->s_list, &super_blocks);
    spinlock_release(&super_blocks_lock);
    
    return sb;
}

struct nameidata {
    struct dentry *dentry;
    struct vfsmount *mnt;
};

 
int path_walk(const char *name, struct nameidata *nd) {
    struct dentry *dentry;
    struct vfsmount *mnt;
    
    if (!name || !nd) return -1;
     
    if (name[0] == '/') {
        dentry = root_dentry;
        mnt = root_mnt;
        name++;  
    } else {
        if (current_process && current_process->cwd) {
            dentry = current_process->cwd;
            mnt = current_process->cwd_mnt ? current_process->cwd_mnt : root_mnt;
        } else {
            dentry = root_dentry;
            mnt = root_mnt;
        }
    }
    
    while (*name) {
        const char *start = name;
        while (*name && *name != '/') name++;
        int len = name - start;
        
        if (len == 0) {
            if (*name) name++;  
            continue;
        }
        
        char component[256];
        if (len >= 256) return -1;
        memcpy(component, start, len);
        component[len] = 0;
        
        if (strcmp(component, ".") == 0) {
            if (*name) name++;
            continue;
        }
        if (strcmp(component, "..") == 0) {
             
            if (dentry != root_dentry && dentry->d_parent) {
                dentry = dentry->d_parent;
            }
            if (*name) name++;
            continue;
        }
        
        struct dentry *child = d_lookup(dentry, component);
        if (!child) {
             
            if (!dentry->d_inode || !dentry->d_inode->i_op || !dentry->d_inode->i_op->lookup) {
                return -1;  
            }
            
            struct dentry *new_dentry = alloc_dentry(dentry, component);
            if (!new_dentry) return -1;
            
            struct dentry *res = dentry->d_inode->i_op->lookup(dentry->d_inode, new_dentry);
            if (res) {
                 
                kfree(new_dentry);
                child = res;
            } else {
                 
                child = new_dentry;
            }
        }
        
        follow_mount(&mnt, &child);
        
        dentry = child;
        if (*name) name++;
    }
    
    nd->dentry = dentry;
    nd->mnt = mnt;
    return 0;
}

static int vfs_resolve_parent(const char *path, struct dentry **parent_out, const char **name_out) {
    int len = strlen(path);
    int last_slash = -1;
    for(int i=len-1; i>=0; i--) if(path[i] == '/') { last_slash = i; break; }
    
    struct dentry *parent = 0;
    const char *name = 0;
    
    if (last_slash == -1) {
        if (current_process && current_process->cwd) {
            parent = current_process->cwd;
        } else {
            parent = root_dentry; 
        }
        name = path;
    } else if (last_slash == 0) {
        parent = root_dentry;
        name = path + 1;
    } else {
        char *p_path = (char*)kmalloc(last_slash + 1);
        if (!p_path) return -1;
        memcpy(p_path, path, last_slash);
        p_path[last_slash] = 0;
        
        struct nameidata nd_parent;
        if (path_walk(p_path, &nd_parent) == 0) {
            parent = nd_parent.dentry;
        }
        kfree(p_path);
        name = path + last_slash + 1;
    }
    
    if (!parent || !parent->d_inode) return -1;
    
    *parent_out = parent;
    *name_out = name;
    return 0;
}

int vfs_open(const char *path, int flags, int mode) {
     
    int open_count = 0;
    for (int i=0; i<MAX_FILES; i++) {
        if (current_process->fd_table[i]) open_count++;
    }
    if (open_count >= MAX_FILES) return -1;  

    struct nameidata nd;
    int err = path_walk(path, &nd);
    
    struct dentry *dentry = 0;
    
    if (err != 0) {
         
        if (flags & O_CREAT) {
             struct dentry *parent;
             const char *name;
             if (vfs_resolve_parent(path, &parent, &name) != 0) return -1;
             
              
             dentry = alloc_dentry(parent, name);
             if (!dentry) return -1;
             
              
             if (!parent->d_inode->i_op || !parent->d_inode->i_op->create) {
                  
                 return -1;
             }
             
             err = parent->d_inode->i_op->create(parent->d_inode, dentry, mode);
             if (err) return err;
             
              
        } else {
            return -1;  
        }
    } else {
        dentry = nd.dentry;
        
        if (!dentry->d_inode) {
             
            if (flags & O_CREAT) {
                 
                struct dentry *parent = dentry->d_parent;
                if (!parent || !parent->d_inode || !parent->d_inode->i_op || !parent->d_inode->i_op->create) {
                    return -1;
                }
                
                err = parent->d_inode->i_op->create(parent->d_inode, dentry, mode);
                if (err) return err;
                
                 
                if (!dentry->d_inode) return -1;
            } else {
                return -1;  
            }
        } else {
             
             
            if ((flags & O_CREAT) && (flags & O_EXCL)) return -1;  
            
             
            if (S_ISDIR(dentry->d_inode->i_mode) && (flags & O_WRONLY || flags & O_RDWR)) return -1;  
        }
    }
    
    if (!dentry || !dentry->d_inode) return -1;
    struct inode *inode = dentry->d_inode;
    
    if (S_ISDIR(inode->i_mode) && !(flags & O_DIRECTORY) && !(flags & O_RDONLY)) {
         
        return -1;
    }
    
     
    struct file *f = (struct file *)kmalloc(sizeof(struct file));
    if (!f) return -1;
    memset(f, 0, sizeof(struct file));
    
    f->f_dentry = dentry;
    f->f_op = inode->i_fop;
    f->f_mode = mode;
    f->f_flags = flags;
    f->f_count = 1;
    f->f_pos = 0;
    
    if (f->f_op && f->f_op->open) {
        err = f->f_op->open(inode, f);
        if (err) {
            kfree(f);
            return err;
        }
    }
    
    for(int i=0; i<MAX_FILES; i++) {
        if (!current_process->fd_table[i]) {
            current_process->fd_table[i] = f;
            return i;
        }
    }
    
    kfree(f);
    return -1;
}

int vfs_close(int fd) {
    kprint_str("DEBUG: vfs_close enter\n");
    if (fd < 0 || fd >= MAX_FILES) return -1;
    struct file *f = current_process->fd_table[fd];
    if (!f) return -1;
    
    current_process->fd_table[fd] = 0;

    if (f->f_op && f->f_op->release) {
        f->f_op->release(f->f_dentry->d_inode, f);
    }
    
    f->f_count--;
    if (f->f_count <= 0) {
        kprint_str("DEBUG: freeing file ptr=");
        kprint_hex((uint64_t)f);
        kprint_str("\n");
        kfree(f);
        kprint_str("DEBUG: freed file\n");
    }
    
    kprint_str("DEBUG: vfs_close exit\n");
    return 0;
}

int vfs_read(int fd, char *buf, int count) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    struct file *f = current_process->fd_table[fd];
    if (!f) return -1;
    
    if (f->f_op && f->f_op->read) {
        return f->f_op->read(f, buf, count, &f->f_pos);
    }
    return -1;
}

int vfs_write(int fd, const char *buf, int count) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    struct file *f = current_process->fd_table[fd];
    if (!f) return -1;
    
    if (f->f_op && f->f_op->write) {
        return f->f_op->write(f, buf, count, &f->f_pos);
    }
    return -1;
}

int vfs_lseek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    struct file *f = current_process->fd_table[fd];
    if (!f) return -1;
    
    int new_pos = f->f_pos;
    switch(whence) {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos += offset; break;
        case SEEK_END: new_pos = f->f_dentry->d_inode->i_size + offset; break;
        default: return -1;
    }
    
    if (new_pos < 0) return -1;
    f->f_pos = new_pos;
    
    if (f->f_op && f->f_op->llseek) {
        return f->f_op->llseek(f, offset, whence);
    }
    
    return new_pos;
}

int vfs_dup(int oldfd) {
    if (oldfd < 0 || oldfd >= MAX_FILES) return -1;
    struct file *f = current_process->fd_table[oldfd];
    if (!f) return -1;
    
    for(int i=0; i<MAX_FILES; i++) {
        if (!current_process->fd_table[i]) {
            f->f_count++;
            current_process->fd_table[i] = f;
            return i;
        }
    }
    return -1;
}

int vfs_readdir(int fd, struct dirent *dirp, int count) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    struct file *f = current_process->fd_table[fd];
    if (!f) return -1;
    if (!S_ISDIR(f->f_dentry->d_inode->i_mode)) return -1;
    
    if (f->f_op && f->f_op->readdir) {
        return f->f_op->readdir(f, dirp, count);  
    }
    return -1;
}

int vfs_mknod(const char *path, int mode, int dev) {
     
    char parent_path[MAX_PATH_LEN];
    char name[MAX_FILENAME_LEN];
    
    int len = strlen(path);
    int i = len - 1;
    while(i >= 0 && path[i] != '/') i--;
    
    if (i < 0) return -1;  
    
    if (i == 0) {
        strcpy(parent_path, "/");
    } else {
        strncpy(parent_path, path, i);
        parent_path[i] = 0;
    }
    
    strcpy(name, path + i + 1);
    
     
    struct nameidata nd;
    if (path_walk(parent_path, &nd) != 0) return -1;
    
    struct dentry *d_parent = nd.dentry;
    if (!d_parent || !d_parent->d_inode) return -1;
    
     
    if (d_parent->d_inode->i_op && d_parent->d_inode->i_op->mknod) {
         
         
        struct dentry *d_child = alloc_dentry(d_parent, name);
        if (!d_child) return -1;
        
        int ret = d_parent->d_inode->i_op->mknod(d_parent->d_inode, d_child, mode, dev);
        if (ret != 0) {
             
             
            return ret;
        }
        return 0;
    }
    
    return -1;
}

int vfs_mkdir(const char *path, int mode) {
    struct dentry *parent;
    const char *name;
    if (vfs_resolve_parent(path, &parent, &name) != 0) return -1;
    
    struct dentry *dentry = alloc_dentry(parent, name);
    if (!dentry) return -1;
    
    if (!parent->d_inode->i_op || !parent->d_inode->i_op->mkdir) {
        return -1;
    }
    
    return parent->d_inode->i_op->mkdir(parent->d_inode, dentry, mode);
}

int vfs_chdir(const char *path) {
    struct nameidata nd;
    if (path_walk(path, &nd) != 0) return -1;
    
    struct dentry *dentry = nd.dentry;
    if (!dentry || !dentry->d_inode) return -1;
    
    if (!S_ISDIR(dentry->d_inode->i_mode)) return -1;  
    
     
    if (current_process) {
        current_process->cwd = dentry;
        current_process->cwd_mnt = nd.mnt;
    }
    
    return 0;
}

void vfs_init() {
    spinlock_init(&file_systems_lock);
    spinlock_init(&super_blocks_lock);
    spinlock_init(&dcache_lock);
    spinlock_init(&vfsmount_lock);
    
    for(int i=0; i<DENTRY_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&dentry_hashtable[i]);
    }
    
    kprint_str("VFS Initialized\n");
}

struct dentry *vfs_lookup(const char *path) {
    struct nameidata nd;
    if (path_walk(path, &nd) == 0) return nd.dentry;
    return 0;
}

int vfs_stat(const char *path, void *stat_buf) {
    if (!path || !stat_buf) return -1;
    
    struct dentry *dentry = vfs_lookup(path);
    if (!dentry || !dentry->d_inode) return -1;
    
    struct inode *inode = dentry->d_inode;
    struct kstat *ks = (struct kstat*)stat_buf;
    
    ks->ino = inode->i_ino;
    ks->mode = inode->i_mode;
    ks->nlink = inode->i_nlink;
    ks->uid = inode->i_uid;
    ks->gid = inode->i_gid;
    ks->size = inode->i_size;
    ks->atime = inode->i_atime.tv_sec;
    ks->mtime = inode->i_mtime.tv_sec;
    ks->ctime = inode->i_ctime.tv_sec;
    ks->blocks = inode->i_blocks;
    ks->blksize = (1 << inode->i_blkbits);
    
    return 0;
}

int vfs_unlink(const char *path) {
    struct dentry *parent;
    const char *name;
    if (vfs_resolve_parent(path, &parent, &name) != 0) return -1;
    
    if (!parent->d_inode->i_op || !parent->d_inode->i_op->unlink) return -1;
    
    struct dentry *dentry = d_lookup(parent, name);
    if (!dentry) {
        dentry = alloc_dentry(parent, name);
        if (!dentry) return -1;
        if (parent->d_inode->i_op->lookup) {
             struct dentry *res = parent->d_inode->i_op->lookup(parent->d_inode, dentry);
             if (res) dentry = res;
        }
    }
    
    if (!dentry->d_inode) return -1;  
    
    return parent->d_inode->i_op->unlink(parent->d_inode, dentry);
}

int vfs_rmdir(const char *path) {
    struct dentry *parent;
    const char *name;
    if (vfs_resolve_parent(path, &parent, &name) != 0) return -1;
    
    if (!parent->d_inode->i_op || !parent->d_inode->i_op->rmdir) return -1;
    
    struct dentry *dentry = d_lookup(parent, name);
    if (!dentry) {
        dentry = alloc_dentry(parent, name);
        if (!dentry) return -1;
        if (parent->d_inode->i_op->lookup) {
             struct dentry *res = parent->d_inode->i_op->lookup(parent->d_inode, dentry);
             if (res) dentry = res;
        }
    }
    
    if (!dentry->d_inode) return -1;  
    if (!S_ISDIR(dentry->d_inode->i_mode)) return -1;  
    
    return parent->d_inode->i_op->rmdir(parent->d_inode, dentry);
}

int vfs_rename(const char *oldpath, const char *newpath) {
    struct dentry *old_parent, *new_parent;
    const char *old_name, *new_name;
    
    if (vfs_resolve_parent(oldpath, &old_parent, &old_name) != 0) return -1;
    if (vfs_resolve_parent(newpath, &new_parent, &new_name) != 0) return -1;
    
    if (!old_parent->d_inode->i_op || !old_parent->d_inode->i_op->rename) return -1;
    
    if (old_parent->d_sb != new_parent->d_sb) return -1;  
    
    struct dentry *old_dentry = d_lookup(old_parent, old_name);
    if (!old_dentry) {
         old_dentry = alloc_dentry(old_parent, old_name);
         if (old_parent->d_inode->i_op->lookup) {
             struct dentry *res = old_parent->d_inode->i_op->lookup(old_parent->d_inode, old_dentry);
             if (res) old_dentry = res;
         }
    }
    if (!old_dentry->d_inode) return -1;
    
    struct dentry *new_dentry = d_lookup(new_parent, new_name);
    if (!new_dentry) {
        new_dentry = alloc_dentry(new_parent, new_name);
        if (new_parent->d_inode->i_op->lookup) {
             struct dentry *res = new_parent->d_inode->i_op->lookup(new_parent->d_inode, new_dentry);
             if (res) new_dentry = res;
        }
    }
    
    return old_parent->d_inode->i_op->rename(old_parent->d_inode, old_dentry, new_parent->d_inode, new_dentry);
}

int vfs_getcwd(char *buf, int size) {
    if (!buf || size <= 0) return -1;
    
    struct dentry *dentry;
    struct vfsmount *mnt;
    
    if (current_process && current_process->cwd) {
        dentry = current_process->cwd;
        mnt = current_process->cwd_mnt;
    } else {
        dentry = root_dentry;
        mnt = root_mnt;
    }
    (void)mnt;  
    
    if (!dentry) return -1;
    
    char *temp = (char*)kmalloc(size);
    if (!temp) return -1;
    
    int pos = size - 1;
    temp[pos] = 0;
    
    if (dentry == root_dentry) {
        buf[0] = '/';
        buf[1] = 0;
        kfree(temp);
        return 0;
    }
    
    while (dentry != root_dentry && dentry != dentry->d_parent) {
        int len = dentry->d_name.len;
        if (pos - len - 1 < 0) {
            kfree(temp);
            return -1;  
        }
        
        pos -= len;
        memcpy(temp + pos, dentry->d_name.name, len);
        pos--;
        temp[pos] = '/';
        
        dentry = dentry->d_parent;
    }
    
    strcpy(buf, temp + pos);
    kfree(temp);
    return 0;
}
