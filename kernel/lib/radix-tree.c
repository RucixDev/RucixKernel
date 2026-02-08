#include "radix-tree.h"
#include "heap.h"
#include "string.h"

static struct radix_tree_node *radix_tree_node_alloc() {
    struct radix_tree_node *ret = (struct radix_tree_node *)kmalloc(sizeof(struct radix_tree_node));
    if (ret) {
        memset(ret, 0, sizeof(struct radix_tree_node));
    }
    return ret;
}

static void radix_tree_node_free(struct radix_tree_node *node) {
    kfree(node);
}

void radix_tree_init(struct radix_tree_root *root) {
    root->height = 0;
    root->rnode = 0;
}

static void radix_tree_shrink(struct radix_tree_root *root) {
    while (root->height > 0) {
        struct radix_tree_node *node = root->rnode;
        if (!node) {
            root->height = 0;
            break;
        }

        if (node->count != 1) break;
        if (!node->slots[0]) break;

        if (root->height <= 1) break;
        
        void *child = node->slots[0];
        root->rnode = (struct radix_tree_node *)child;
        root->height--;
        radix_tree_node_free(node);
    }
}

static int radix_tree_extend(struct radix_tree_root *root, unsigned long index) {
    struct radix_tree_node *node;
    unsigned int height;

     
    height = root->height + 1;
    while (index > ((1UL << (height * RADIX_TREE_MAP_SHIFT)) - 1))
        height++;

    if (root->rnode == 0) {
        root->height = height;
    }
    
    while (root->height < height) {
        node = radix_tree_node_alloc();
        if (!node) return -1;
        
        node->slots[0] = root->rnode;
        node->count = 1;  
        root->rnode = node;
        root->height++;
    }
    return 0;
}

int radix_tree_insert(struct radix_tree_root *root, unsigned long index, void *item) {
    struct radix_tree_node *node = 0, *slot = 0;
    unsigned int height, shift;
    int offset;

     
    if (root->rnode == 0 && root->height == 0) {
        unsigned long tmp = index;
        unsigned int h = 0;
        while (tmp > 0) {
            tmp >>= RADIX_TREE_MAP_SHIFT;
            h++;
        }
        if (h == 0) h = 1;  
        root->height = h;
    }
    
     
    unsigned long max_index = (1UL << (root->height * RADIX_TREE_MAP_SHIFT)) - 1;
    if (index > max_index) {
        if (radix_tree_extend(root, index)) return -1;
    }

    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    if (!root->rnode) {
        root->rnode = radix_tree_node_alloc();
        if (!root->rnode) return -1;
    }
    
    node = root->rnode;
    
    while (height > 0) {
        if (shift >= 64) offset = 0;  
        else offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        if (height == 1) {
            if (node->slots[offset]) return -1;  
            node->slots[offset] = item;
            node->count++;
            return 0;
        }
        
        if (!node->slots[offset]) {
            slot = radix_tree_node_alloc();
            if (!slot) return -1;
            node->slots[offset] = slot;
            node->count++;
        }
        
        node = (struct radix_tree_node *)node->slots[offset];
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }
    
    return -1;  
}

void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index) {
    unsigned int height, shift;
    struct radix_tree_node *node;
    int offset;

    node = root->rnode;
    height = root->height;
    if (height == 0 || !node) return 0;
    
    unsigned long max_index = (1UL << (height * RADIX_TREE_MAP_SHIFT)) - 1;
    if (index > max_index) return 0;

    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    while (height > 0) {
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        if (!node->slots[offset]) return 0;
        
        if (height == 1) {
            return node->slots[offset];
        }
        
        node = (struct radix_tree_node *)node->slots[offset];
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }
    return 0;
}

void *radix_tree_delete(struct radix_tree_root *root, unsigned long index) {
    struct radix_tree_node *stack[16];  
    int offsets[16];
    int top = 0;
    
    unsigned int height, shift;
    struct radix_tree_node *node;
    int offset;

    node = root->rnode;
    height = root->height;
    if (height == 0 || !node) return 0;
    
    unsigned long max_index = (1UL << (height * RADIX_TREE_MAP_SHIFT)) - 1;
    if (index > max_index) return 0;

    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
     
    while (height > 0) {
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        stack[top] = node;
        offsets[top] = offset;
        top++;
        
        if (!node->slots[offset]) return 0;  
        
        if (height == 1) {
             void *item = node->slots[offset];
             node->slots[offset] = 0;
             node->count--;
             
             while (top > 0) {
                 node = stack[top-1];
                 if (node->count > 0) break;  

                 if (top > 1) {
                     struct radix_tree_node *parent = stack[top-2];
                     int p_off = offsets[top-2];
                     parent->slots[p_off] = 0;
                     parent->count--;
                     radix_tree_node_free(node);
                     top--;
                 } else {
                     radix_tree_node_free(node);
                     root->rnode = 0;
                     root->height = 0;  
                     top--;
                 }
             }
             
             radix_tree_shrink(root);
             
             return item;
        }
        
        node = (struct radix_tree_node *)node->slots[offset];
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }
    return 0;
}
