#include "vfs.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "fs/buffer.h"
#include "drivers/blockdev.h"

struct fat_boot_sector {
    uint8_t jump[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t vol_id;
    char vol_label[11];
    char fs_type[8];
} __attribute__((packed));

struct fat_dir_entry {
    char name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t acc_date;
    uint16_t cluster_high;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed));

struct fat32_sb_info {
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t root_cluster;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t fat_size;  
    uint32_t total_sectors;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t hidden_sectors;
};

static uint32_t cluster_to_sector(struct fat32_sb_info *sbi, uint32_t cluster) {
    return sbi->data_start_sector + (cluster - 2) * sbi->sectors_per_cluster;
}

static uint32_t fat32_next_cluster(struct super_block *sb, uint32_t cluster) {
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    uint32_t fat_sector = sbi->fat_start_sector + (cluster * 4) / sb->s_blocksize;
    uint32_t fat_offset = (cluster * 4) % sb->s_blocksize;
    
    struct buffer_head *bh = bread(sb->s_bdev, fat_sector, sb->s_blocksize);
    if (!bh) return 0x0FFFFFFF;  
    
    uint32_t next_cluster = *(uint32_t*)(bh->b_data + fat_offset);
    next_cluster &= 0x0FFFFFFF;  
    
    brelse(bh);
    return next_cluster;
}

 
static int fat32_write_fat_entry(struct super_block *sb, uint32_t cluster, uint32_t value) {
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    uint32_t fat_sector = sbi->fat_start_sector + (cluster * 4) / sb->s_blocksize;
    uint32_t fat_offset = (cluster * 4) % sb->s_blocksize;
    
    struct buffer_head *bh = bread(sb->s_bdev, fat_sector, sb->s_blocksize);
    if (!bh) return -1;
    
    uint32_t *entry = (uint32_t*)(bh->b_data + fat_offset);
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);
    
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

static uint32_t fat32_find_free_cluster(struct super_block *sb) {
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    uint32_t cluster = 2;
    uint32_t max_cluster = sbi->total_sectors / sbi->sectors_per_cluster; 
    
    for (; cluster < max_cluster; cluster++) {
        uint32_t val = fat32_next_cluster(sb, cluster);
        if (val == 0) return cluster;
    }
    return 0;
}

static void fat32_free_cluster(struct super_block *sb, uint32_t cluster) {
    fat32_write_fat_entry(sb, cluster, 0);
}

static uint32_t fat32_alloc_cluster(struct super_block *sb, uint32_t prev_cluster) {
    uint32_t new_cluster = fat32_find_free_cluster(sb);
    if (new_cluster == 0) return 0;
    
    if (fat32_write_fat_entry(sb, new_cluster, 0x0FFFFFFF) < 0) return 0;
    
    if (prev_cluster != 0) {
        if (fat32_write_fat_entry(sb, prev_cluster, new_cluster) < 0) {
            fat32_write_fat_entry(sb, new_cluster, 0);
            return 0;
        }
    }
    
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    uint32_t sector = cluster_to_sector(sbi, new_cluster);
    for (uint32_t i=0; i<sbi->sectors_per_cluster; i++) {
        struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
        if (bh) {
            memset(bh->b_data, 0, 512);
            mark_buffer_dirty(bh);
            sync_dirty_buffer(bh);
            brelse(bh);
        }
    }
    
    return new_cluster;
}

 
static int fat32_readdir(struct file *file, void *dirent, int count);
static int fat32_read(struct file *file, char *buf, int count, uint64_t *offset);
static int fat32_write(struct file *file, const char *buf, int count, uint64_t *offset);
static struct dentry *fat32_lookup(struct inode *dir, struct dentry *dentry);

static struct file_operations fat32_dir_ops = {
    .readdir = fat32_readdir,
    .open = 0,
    .release = 0
};

static struct file_operations fat32_file_ops = {
    .read = fat32_read,
    .write = fat32_write,
    .open = 0,
    .release = 0
};

static int fat32_create(struct inode *dir, struct dentry *dentry, int mode);
static int fat32_mkdir(struct inode *dir, struct dentry *dentry, int mode);
static int fat32_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev);

static struct inode_operations fat32_dir_inode_ops = {
    .lookup = fat32_lookup,
    .create = fat32_create,
    .mkdir = fat32_mkdir,
    .mknod = fat32_mknod
};

static struct inode *fat32_get_inode(struct super_block *sb, uint32_t cluster, uint32_t size, int is_dir) {
    struct inode *inode = new_inode(sb);
    if (!inode) return 0;
    
    inode->i_ino = cluster;
    inode->i_size = size;
    
    if (is_dir) {
        inode->i_mode = S_IFDIR | 0755;
        inode->i_op = &fat32_dir_inode_ops;
        inode->i_fop = &fat32_dir_ops;
    } else {
        inode->i_mode = S_IFREG | 0644;
        inode->i_op = 0;
        inode->i_fop = &fat32_file_ops;
    }
    
    return inode;
}

 
static void fat32_format_name(const char *name, char *out) {
    memset(out, ' ', 11);
    int i = 0, j = 0;
    
     
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[j++] = c;
    }
    
     
    while (name[i] && name[i] != '.') i++;
    
    if (name[i] == '.') {
        i++;  
        j = 8;
        while (name[i] && j < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[j++] = c;
        }
    }
}

static int fat32_add_entry(struct inode *dir, const char *name, uint32_t cluster, uint32_t size, int is_dir) {
    struct super_block *sb = dir->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    char fat_name[11];
    fat32_format_name(name, fat_name);
    
    uint32_t dir_cluster = dir->i_ino;
    
    while (1) {
        uint32_t sector = cluster_to_sector(sbi, dir_cluster);
        for (uint32_t i = 0; i < sbi->sectors_per_cluster; i++) {
            struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
            if (!bh) return -1;
            
            struct fat_dir_entry *entries = (struct fat_dir_entry *)bh->b_data;
            for (int j = 0; j < 512 / 32; j++) {
                if (entries[j].name[0] == 0 || (unsigned char)entries[j].name[0] == 0xE5) {
                     
                    memset(&entries[j], 0, sizeof(struct fat_dir_entry));
                    memcpy(entries[j].name, fat_name, 11);
                    entries[j].attr = is_dir ? 0x10 : 0x20;
                    entries[j].cluster_high = (cluster >> 16) & 0xFFFF;
                    entries[j].cluster_low = cluster & 0xFFFF;
                    entries[j].size = size;
                    
                    mark_buffer_dirty(bh);
                    sync_dirty_buffer(bh);
                    brelse(bh);
                    return 0;
                }
            }
            brelse(bh);
        }
        
        uint32_t next = fat32_next_cluster(sb, dir_cluster);
        if (next >= 0x0FFFFFF8) {
             
            next = fat32_alloc_cluster(sb, dir_cluster);
            if (next == 0) return -1;
        }
        dir_cluster = next;
    }
}

static int fat32_create(struct inode *dir, struct dentry *dentry, int mode) {
    (void)mode;
    struct super_block *sb = dir->i_sb;
    
     
    uint32_t cluster = fat32_alloc_cluster(sb, 0);
    if (cluster == 0) return -1;
    
     
    if (fat32_add_entry(dir, dentry->d_name.name, cluster, 0, 0) < 0) {
        fat32_free_cluster(sb, cluster);
        return -1;
    }
    
    struct inode *inode = fat32_get_inode(sb, cluster, 0, 0);
    if (!inode) return -1;
    
    d_instantiate(dentry, inode);
    return 0;
}

static int fat32_mkdir(struct inode *dir, struct dentry *dentry, int mode) {
    (void)mode;
    struct super_block *sb = dir->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    
     
    uint32_t cluster = fat32_alloc_cluster(sb, 0);
    if (cluster == 0) return -1;
    
     
    if (fat32_add_entry(dir, dentry->d_name.name, cluster, 0, 1) < 0) {
        fat32_free_cluster(sb, cluster);
        return -1;
    }
    
     
    struct inode *new_dir = fat32_get_inode(sb, cluster, 0, 1);
    if (!new_dir) return -1;
    
     
    fat32_add_entry(new_dir, ".", cluster, 0, 1);
    
     
    uint32_t parent_cluster = dir->i_ino;
    if (parent_cluster == sbi->root_cluster) parent_cluster = 0;
    fat32_add_entry(new_dir, "..", parent_cluster, 0, 1);
    
    d_instantiate(dentry, new_dir);
    return 0;
}

static int fat32_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev) {
    (void)dev;
     
    if (!S_ISREG(mode)) return -1;
    return fat32_create(dir, dentry, mode);
}

 

static int fat32_read(struct file *file, char *buf, int count, uint64_t *offset) {
    struct inode *inode = file->f_dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    
    if (*offset >= inode->i_size) return 0;
    if (*offset + count > inode->i_size) count = inode->i_size - *offset;
    
    uint32_t cluster = inode->i_ino;  
    uint32_t cluster_size = sbi->bytes_per_cluster;
    
     
    uint32_t skip_clusters = *offset / cluster_size;
    uint32_t cluster_offset = *offset % cluster_size;
    
    for (uint32_t i = 0; i < skip_clusters; i++) {
        cluster = fat32_next_cluster(sb, cluster);
        if (cluster >= 0x0FFFFFF8) return 0;  
    }
    
    int read = 0;
    while (read < count) {
        if (cluster >= 0x0FFFFFF8) break;
        
        uint32_t sector = cluster_to_sector(sbi, cluster);
        
        for (uint32_t i = 0; i < sbi->sectors_per_cluster; i++) {
            if (read >= count) break;
            
             
            if (cluster_offset >= 512) {
                cluster_offset -= 512;
                continue;
            }
            
            struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
            if (!bh) return -1;  
            
            int to_copy = 512 - cluster_offset;
            if (to_copy > count - read) to_copy = count - read;
            
            memcpy(buf + read, bh->b_data + cluster_offset, to_copy);
            
            brelse(bh);
            
            read += to_copy;
            cluster_offset = 0;  
            *offset += to_copy;
        }
        
        cluster = fat32_next_cluster(sb, cluster);
    }
    
    return read;
}

static int fat32_update_inode_size(struct inode *inode, struct dentry *dentry) {
    if (!dentry || !dentry->d_parent) return -1;
    struct inode *dir = dentry->d_parent->d_inode;
    struct super_block *sb = inode->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    
    uint32_t cluster = dir->i_ino;
    
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(sbi, cluster);
        for (uint32_t i = 0; i < sbi->sectors_per_cluster; i++) {
            struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
            if (!bh) return -1;
            
            struct fat_dir_entry *entries = (struct fat_dir_entry *)bh->b_data;
            for (int j = 0; j < 512 / 32; j++) {
                if (entries[j].name[0] == 0) { brelse(bh); return -1; }
                if ((unsigned char)entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == 0x0F) continue;
                
                uint32_t entry_cluster = ((uint32_t)entries[j].cluster_high << 16) | entries[j].cluster_low;
                if (entry_cluster == inode->i_ino) {
                    entries[j].size = inode->i_size;
                    mark_buffer_dirty(bh);
                    sync_dirty_buffer(bh);
                    brelse(bh);
                    return 0;
                }
            }
            brelse(bh);
        }
        cluster = fat32_next_cluster(sb, cluster);
    }
    return -1;
}

static int fat32_write(struct file *file, const char *buf, int count, uint64_t *offset) {
    struct inode *inode = file->f_dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    
    if (count == 0) return 0;
    
    uint32_t cluster = inode->i_ino;
    uint32_t bytes_per_cluster = sbi->sectors_per_cluster * 512;
    uint32_t target_pos = *offset;
    
     
    while (target_pos >= bytes_per_cluster) {
        uint32_t next = fat32_next_cluster(sb, cluster);
        if (next >= 0x0FFFFFF8) {
            next = fat32_alloc_cluster(sb, cluster);
            if (next == 0) return -1;
        }
        cluster = next;
        target_pos -= bytes_per_cluster;
    }
    
    int written = 0;
    while (written < count) {
        uint32_t sector = cluster_to_sector(sbi, cluster);
        uint32_t offset_in_cluster = target_pos;
        
        for (uint32_t i = 0; i < sbi->sectors_per_cluster; i++) {
            if (written >= count) break;
            
            if (offset_in_cluster >= 512) {
                offset_in_cluster -= 512;
                continue;
            }
            
            struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
            if (!bh) return -1;
            
            int to_write = 512 - offset_in_cluster;
            if (to_write > count - written) to_write = count - written;
            
            memcpy(bh->b_data + offset_in_cluster, buf + written, to_write);
            
            mark_buffer_dirty(bh);
            sync_dirty_buffer(bh);
            brelse(bh);
            
            written += to_write;
            target_pos = 0; 
            offset_in_cluster = 0;
            *offset += to_write;
            
            if (*offset > inode->i_size) {
                inode->i_size = *offset;
                fat32_update_inode_size(inode, file->f_dentry);
            }
        }
        
        if (written < count) {
             uint32_t next = fat32_next_cluster(sb, cluster);
             if (next >= 0x0FFFFFF8) {
                 next = fat32_alloc_cluster(sb, cluster);
                 if (next == 0) return written;
             }
             cluster = next;
        }
    }
    
    return written;
}

static int fat32_readdir(struct file *file, void *dirent, int count) {
    (void)count;
    struct inode *inode = file->f_dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    struct dirent *d = (struct dirent *)dirent;
    
     
    if (file->f_pos == 0) {
        strcpy(d->d_name, ".");
        d->d_type = DT_DIR;
        file->f_pos++;
        return 1;
    }
    if (file->f_pos == 1) {
        strcpy(d->d_name, "..");
        d->d_type = DT_DIR;
        file->f_pos++;
        return 1;
    }
    
     
    uint32_t cluster = inode->i_ino;
    uint32_t entry_idx = 0;
    uint32_t target_idx = file->f_pos - 2;
    
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(sbi, cluster);
        
        for (uint32_t i = 0; i < sbi->sectors_per_cluster; i++) {
            struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
            if (!bh) return -1;
            
            struct fat_dir_entry *entries = (struct fat_dir_entry *)bh->b_data;
            for (int j = 0; j < 512 / 32; j++) {
                if (entries[j].name[0] == 0) {  
                    brelse(bh);
                    return 0; 
                }
                if ((unsigned char)entries[j].name[0] == 0xE5) continue;  
                if (entries[j].attr == 0x0F) continue;  
                
                if (entry_idx == target_idx) {
                     
                     
                    char name[13];
                    int k = 0, l = 0;
                    for (; k < 8 && entries[j].name[k] != ' '; k++) name[l++] = entries[j].name[k];
                    if (entries[j].attr != 0x10 && entries[j].name[8] != ' ') {  
                        name[l++] = '.';
                        for (k = 8; k < 11 && entries[j].name[k] != ' '; k++) name[l++] = entries[j].name[k];
                    }
                    name[l] = 0;
                    
                     
                    for(int z=0; z<l; z++) {
                        if (name[z] >= 'A' && name[z] <= 'Z') name[z] += 32;
                    }
                    
                    strncpy(d->d_name, name, 255);
                    d->d_ino = ((uint32_t)entries[j].cluster_high << 16) | entries[j].cluster_low;
                    d->d_type = (entries[j].attr & 0x10) ? DT_DIR : DT_REG;
                    
                    file->f_pos++;
                    brelse(bh);
                    return 1;
                }
                entry_idx++;
            }
            brelse(bh);
        }
        cluster = fat32_next_cluster(sb, cluster);
    }
    
    return 0;
}

static struct dentry *fat32_lookup(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)sb->s_fs_info;
    
    uint32_t cluster = dir->i_ino;
    
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(sbi, cluster);
        
        for (uint32_t i = 0; i < sbi->sectors_per_cluster; i++) {
            struct buffer_head *bh = bread(sb->s_bdev, sector + i, 512);
            if (!bh) return 0;
            
            struct fat_dir_entry *entries = (struct fat_dir_entry *)bh->b_data;
            for (int j = 0; j < 512 / 32; j++) {
                if (entries[j].name[0] == 0) { brelse(bh); return 0; }  
                if ((unsigned char)entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == 0x0F) continue;
                
                char name[13];
                int k = 0, l = 0;
                for (; k < 8 && entries[j].name[k] != ' '; k++) name[l++] = entries[j].name[k];
                if (entries[j].attr != 0x10 && entries[j].name[8] != ' ') {
                     name[l++] = '.';
                     for (k = 8; k < 11 && entries[j].name[k] != ' '; k++) name[l++] = entries[j].name[k];
                }
                name[l] = 0;
                
                if (strcasecmp(name, dentry->d_name.name) == 0) {
                    uint32_t target_cluster = ((uint32_t)entries[j].cluster_high << 16) | entries[j].cluster_low;
                    uint32_t size = entries[j].size;
                    int is_dir = (entries[j].attr & 0x10);
                    
                    struct inode *inode = fat32_get_inode(sb, target_cluster, size, is_dir);
                    d_add(dentry, inode);
                    brelse(bh);
                    return dentry;
                }
            }
            brelse(bh);
        }
        cluster = fat32_next_cluster(sb, cluster);
    }
    
    return 0;
}

 

static void fat32_put_super(struct super_block *sb) {
    if (sb->s_fs_info) kfree(sb->s_fs_info);
}

static struct super_operations fat32_sops = {
    .put_super = fat32_put_super,
};

 

static struct super_block *fat32_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    (void)data;
    (void)flags;
    
    struct gendisk *disk = get_gendisk(dev_name);
    
    if (!disk) {
        kprint_str("[FAT32] Device not found: ");
        kprint_str(dev_name);
        kprint_newline();
        return 0;
    }

    struct super_block *sb = (struct super_block *)kmalloc(sizeof(struct super_block));
    if (!sb) return 0;
    memset(sb, 0, sizeof(struct super_block));
    
    sb->s_bdev = disk;
    sb->s_blocksize = 512;  
    
     
    struct buffer_head *bh = bread(disk, 0, 512);
    if (!bh) {
        kprint_str("[FAT32] Failed to read boot sector\n");
        kfree(sb);
        return 0;
    }
    
    struct fat_boot_sector *bs = (struct fat_boot_sector *)bh->b_data;

     
    uint16_t *magic = (uint16_t *)(bh->b_data + 510);
    if (*magic != 0xAA55) {
        kprint_str("[FAT32] Invalid boot signature\n");
        brelse(bh);
        kfree(sb);
        return 0;
    }

     
    struct fat32_sb_info *sbi = (struct fat32_sb_info *)kmalloc(sizeof(struct fat32_sb_info));
    if (!sbi) {
        brelse(bh);
        kfree(sb);
        return 0;
    }
    memset(sbi, 0, sizeof(struct fat32_sb_info));

    sbi->sectors_per_cluster = bs->sectors_per_cluster;
    sbi->reserved_sectors = bs->reserved_sectors;
    sbi->fat_count = bs->fat_count;
    sbi->hidden_sectors = bs->hidden_sectors;
    sbi->total_sectors = (bs->total_sectors_16 == 0) ? bs->total_sectors_32 : bs->total_sectors_16;
    
     
    sbi->fat_size = (bs->fat_size_16 != 0) ? bs->fat_size_16 : bs->fat_size_32;
    
     
    sbi->bytes_per_cluster = sbi->sectors_per_cluster * sb->s_blocksize;

     
    if (bs->fat_size_16 == 0) {
        sbi->root_cluster = bs->root_cluster;
    } else {
         
        if (sbi->fat_size == 0) { 
             kprint_str("[FAT32] Invalid FAT size\n");
             kfree(sbi);
             brelse(bh);
             kfree(sb);
             return 0;
        }
    }
    
     
    if (bs->fat_size_16 != 0) {
        kprint_str("[FAT32] Warning: Detected FAT12/16, this driver is for FAT32. Proceeding with caution.\n");
    }

    sbi->fat_start_sector = sbi->reserved_sectors;
    sbi->data_start_sector = sbi->reserved_sectors + (sbi->fat_count * sbi->fat_size);
    
    sb->s_fs_info = sbi;
    sb->s_op = &fat32_sops;
    sb->s_type = fs_type;
    sb->s_magic = 0x4d44;
    
    brelse(bh);

    kprint_str("[FAT32] Mounted\n");
    
     
    struct inode *root = fat32_get_inode(sb, sbi->root_cluster, 0, 1);
    if (!root) {
        kfree(sbi);
        kfree(sb);
        return 0;
    }
    
    sb->s_root = alloc_dentry(0, "/");
    if (!sb->s_root) {
        kfree(root);
        kfree(sbi);
        kfree(sb);
        return 0;
    }
    d_add(sb->s_root, root);
    
    return sb;
}

static struct file_system_type fat32_fs_type = {
    .name = "fat32",
    .mount = fat32_mount,
    .kill_sb = 0,
    .next = 0
};

void fat32_init(void) {
    register_filesystem(&fat32_fs_type);
    kprint_str("[FAT32] Filesystem registered\n");
}
