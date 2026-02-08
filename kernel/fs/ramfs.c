#include "vfs.h"
#include "ramfs.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "chardev.h"

 
static struct inode_operations ramfs_dir_inode_ops;
static struct file_operations ramfs_file_ops;
static struct file_operations ramfs_dir_ops;

struct ramfs_inode_info {
    char *data;
    uint64_t capacity;
};

static struct ramfs_inode_info *get_ramfs_inode(struct inode *inode) {
    return (struct ramfs_inode_info *)inode->i_private;
}

static struct inode *ramfs_get_inode(struct super_block *sb, int mode, int dev) {
    struct inode *inode = new_inode(sb);
    if (!inode) return 0;

    inode->i_mode = mode;
    inode->i_rdev = dev;
    inode->i_size = 0;
    inode->i_blocks = 0;
    inode->i_atime.tv_sec = 0;
    inode->i_mtime.tv_sec = 0;
    inode->i_ctime.tv_sec = 0;

    if (S_ISREG(mode)) {
        inode->i_op = 0;
        inode->i_fop = &ramfs_file_ops;
        
        struct ramfs_inode_info *info = (struct ramfs_inode_info*)kmalloc(sizeof(struct ramfs_inode_info));
        if (info) {
            memset(info, 0, sizeof(struct ramfs_inode_info));
            inode->i_private = info;
        } else {
             
            kfree(inode);  
            return 0;
        }
    } else if (S_ISDIR(mode)) {
        inode->i_op = &ramfs_dir_inode_ops;
        inode->i_fop = &ramfs_dir_ops;
    } else if (S_ISCHR(mode)) {
        inode->i_op = 0;
        int major = (dev >> 8) & 0xFF;
        inode->i_fop = chardev_get_fops(major);
    }

    return inode;
}

static int ramfs_read(struct file *file, char *buf, int count, uint64_t *offset) {
    struct inode *inode = file->f_dentry->d_inode;
    struct ramfs_inode_info *info = get_ramfs_inode(inode);
    
    if (*offset >= inode->i_size) return 0;
    
    if (*offset + count > inode->i_size) {
        count = inode->i_size - *offset;
    }
    
    memcpy(buf, info->data + *offset, count);
    *offset += count;
    
    return count;
}

static int ramfs_write(struct file *file, const char *buf, int count, uint64_t *offset) {
    struct inode *inode = file->f_dentry->d_inode;
    struct ramfs_inode_info *info = get_ramfs_inode(inode);
    
    if (*offset + count > info->capacity) {
        uint64_t new_cap = (*offset + count + 4095) & ~4095;  
        char *new_data = (char *)kmalloc(new_cap);
        if (!new_data) return -1;  
        
        if (info->data) {
            memcpy(new_data, info->data, inode->i_size);
            kfree(info->data);
        } else {
            memset(new_data, 0, new_cap);
        }
        
        info->data = new_data;
        info->capacity = new_cap;
    }
    
    memcpy(info->data + *offset, buf, count);
    *offset += count;
    
    if (*offset > inode->i_size) {
        inode->i_size = *offset;
    }
    
    return count;
}

static struct file_operations ramfs_file_ops = {
    .read = ramfs_read,
    .write = ramfs_write,
    .open = 0,
    .release = 0
};

static int ramfs_readdir(struct file *file, void *dirent, int count) {
    (void)count;
    struct dentry *dentry = file->f_dentry;
    struct dirent *d = (struct dirent *)dirent;
    
    if (!dentry || !dentry->d_inode) return -1;
    if (!S_ISDIR(dentry->d_inode->i_mode)) return -1;
    
    int index = 0;
          struct list_head *pos;

          if (file->f_pos == 0) {
        strcpy(d->d_name, ".");
        d->d_ino = (uint64_t)dentry->d_inode;
        d->d_off = 0;
        d->d_reclen = sizeof(struct dirent);
        d->d_type = DT_DIR;
        file->f_pos++;
        return 1;
    }
    
    if (file->f_pos == 1) {
        strcpy(d->d_name, "..");
        d->d_ino = dentry->d_parent ? (uint64_t)dentry->d_parent->d_inode : 0;
        d->d_off = 1;
        d->d_reclen = sizeof(struct dirent);
        d->d_type = DT_DIR;
        file->f_pos++;
        return 1;
    }
    
    index = 2;
    spinlock_acquire(&dentry->d_lock);
    list_for_each(pos, &dentry->d_subdirs) {
        if ((uint64_t)index == file->f_pos) {
            struct dentry *child = list_entry(pos, struct dentry, d_child);
            strncpy(d->d_name, child->d_name.name, 255);
            d->d_ino = (uint64_t)child->d_inode;
            d->d_off = index;
            d->d_reclen = sizeof(struct dirent);
            if (child->d_inode) {
                if (S_ISDIR(child->d_inode->i_mode)) d->d_type = DT_DIR;
                else if (S_ISREG(child->d_inode->i_mode)) d->d_type = DT_REG;
                else d->d_type = DT_UNKNOWN;
            } else {
                d->d_type = DT_UNKNOWN;
            }
            
            file->f_pos++;
            spinlock_release(&dentry->d_lock);
            return 1;
        }
        index++;
    }
    spinlock_release(&dentry->d_lock);
    
    return 0;
}

static struct file_operations ramfs_dir_ops = {
    .readdir = ramfs_readdir,
    .open = 0,
    .release = 0
};

static int ramfs_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev) {
    struct inode *inode = ramfs_get_inode(dir->i_sb, mode, dev);
    if (!inode) return -1;

    if (list_empty(&dentry->d_hash)) {
        d_add(dentry, inode);
    } else {
        d_instantiate(dentry, inode);
    }
    return 0;
}

static int ramfs_create(struct inode *dir, struct dentry *dentry, int mode) {
    return ramfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int ramfs_mkdir(struct inode *dir, struct dentry *dentry, int mode) {
    return ramfs_mknod(dir, dentry, mode | S_IFDIR, 0);
}

static struct dentry *ramfs_lookup(struct inode *dir, struct dentry *dentry) {
    (void)dir;
    d_add(dentry, NULL);  
    return NULL;
}

static struct inode_operations ramfs_dir_inode_ops = {
    .create = ramfs_create,
    .lookup = ramfs_lookup,
    .mkdir = ramfs_mkdir,
    .mknod = ramfs_mknod,
};

static void ramfs_drop_inode(struct inode *inode) {
    if (inode->i_private) {
        struct ramfs_inode_info *info = (struct ramfs_inode_info*)inode->i_private;
        if (info->data) {
            kfree(info->data);
        }
        kfree(info);
        inode->i_private = 0;
    }
}

static void ramfs_kill_sb(struct super_block *sb) {
    if (sb) {
        kfree(sb);
    }
}

static struct super_operations ramfs_ops = {
    .statfs = 0,
    .drop_inode = ramfs_drop_inode,
};

static struct super_block *ramfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    (void)flags; (void)dev_name; (void)data;
    struct super_block *sb = (struct super_block *)kmalloc(sizeof(struct super_block));
    if (!sb) return 0;
    
    memset(sb, 0, sizeof(struct super_block));
    sb->s_blocksize = 4096;
    sb->s_blocksize_bits = 12;
    sb->s_magic = 0x858458f6;  
    sb->s_op = &ramfs_ops;
    sb->s_type = fs_type;
    
    struct inode *inode = ramfs_get_inode(sb, S_IFDIR | 0755, 0);
    sb->s_root = alloc_dentry(0, "/");
    if (!sb->s_root) {
        kfree(sb);
        return 0;
    }
    d_add(sb->s_root, inode);
    
    return sb;
}

static struct file_system_type ramfs_type = {
    .name = "ramfs",
    .mount = ramfs_mount,
    .kill_sb = ramfs_kill_sb,
};

void ramfs_init(void) {
    register_filesystem(&ramfs_type);
}
