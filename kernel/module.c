#include "module.h"
#include "elf.h"
#include "vmm.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "waitqueue.h"
#include "syscall.h"
#include "list.h"

extern struct kernel_symbol __start___ksymtab[];
extern struct kernel_symbol __stop___ksymtab[];

static LIST_HEAD(modules);
static mutex_t module_mutex;
static int module_initialized = 0;

void module_subsystem_init() {
    if (!module_initialized) {
        mutex_init(&module_mutex);
        module_initialized = 1;
        kprint_str("Module Subsystem Initialized.\n");
    }
}

static uint64_t resolve_symbol(const char *name) {
    struct kernel_symbol *k;
    
     
    for (k = __start___ksymtab; k < __stop___ksymtab; k++) {
        if (strcmp(k->name, name) == 0) {
            return k->value;
        }
    }

    module_t *mod;
    struct list_head *pos;
    list_for_each(pos, &modules) {
        mod = list_entry(pos, module_t, list);
        for (unsigned int i = 0; i < mod->num_syms; i++) {
            if (strcmp(mod->syms[i].name, name) == 0) {
                return mod->syms[i].value;
            }
        }
    }

    return 0;
}

static int apply_relocations(Elf64_Shdr *sechdrs, const char *strtab, 
                             unsigned int symindex, unsigned int relsec, 
                             module_t *mod) {
    (void)mod;
    Elf64_Shdr *rel_sec = &sechdrs[relsec];
    Elf64_Shdr *target_sec = &sechdrs[rel_sec->sh_info];
    Elf64_Shdr *sym_sec = &sechdrs[symindex];
    
    Elf64_Rela *rel = (Elf64_Rela *)rel_sec->sh_addr;
    Elf64_Sym *sym = (Elf64_Sym *)sym_sec->sh_addr;
    
    unsigned int num_relocs = rel_sec->sh_size / sizeof(Elf64_Rela);
    
    for (unsigned int i = 0; i < num_relocs; i++, rel++) {
        uint64_t loc = target_sec->sh_addr + rel->r_offset;
        uint64_t sym_val = 0;
        unsigned int type = ELF64_R_TYPE(rel->r_info);
        unsigned int sym_idx = ELF64_R_SYM(rel->r_info);
        
        if (sym_idx != 0) {
            const char *sym_name = strtab + sym[sym_idx].st_name;
            
            if (sym[sym_idx].st_shndx == SHN_UNDEF) {
                 
                sym_val = resolve_symbol(sym_name);
                if (!sym_val) {
                    kprint_str("Module: Undefined symbol: ");
                    kprint_str(sym_name);
                    kprint_newline();
                    return -1;
                }
            } else {
                 
                Elf64_Shdr *s = &sechdrs[sym[sym_idx].st_shndx];
                sym_val = s->sh_addr + sym[sym_idx].st_value;
            }
        }
        
        switch (type) {
            case R_X86_64_NONE:
                break;
            case R_X86_64_64:
                *(uint64_t *)loc = sym_val + rel->r_addend;
                break;
            case R_X86_64_32:
                *(uint32_t *)loc = (uint32_t)(sym_val + rel->r_addend);
                break;
            case R_X86_64_32S:
                *(int32_t *)loc = (int32_t)(sym_val + rel->r_addend);
                break;
            case R_X86_64_PC32:
            case R_X86_64_PLT32:
                *(uint32_t *)loc = (uint32_t)(sym_val + rel->r_addend - loc);
                break;
            default:
                kprint_str("Module: Unknown relocation type: ");
                kprint_dec(type);
                kprint_newline();
                return -1;
        }
    }
    return 0;
}

long sys_init_module(void *module_image, unsigned long len, const char *param_values) {
    (void)param_values;
    if (!module_initialized) module_subsystem_init();
    
    mutex_lock(&module_mutex);
    
    Elf64_Ehdr *hdr = (Elf64_Ehdr *)module_image;
    
     
    if (len < sizeof(Elf64_Ehdr) || 
        hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E' || 
        hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F' ||
        hdr->e_ident[4] != ELFCLASS64 || hdr->e_machine != EM_X86_64 ||
        hdr->e_type != 1) {  
        kprint_str("Module: Invalid ELF header or not relocatable\n");
        mutex_unlock(&module_mutex);
        return -1;
    }
    
    Elf64_Shdr *sechdrs = (Elf64_Shdr *)((uint8_t *)module_image + hdr->e_shoff);
     
    char *secstrings = (char *)module_image + sechdrs[hdr->e_shstrndx].sh_offset;
    (void)secstrings;
    
    uint64_t total_size = 0;
    for (int i = 0; i < hdr->e_shnum; i++) {
        if (sechdrs[i].sh_flags & SHF_ALLOC) {
            uint64_t align = sechdrs[i].sh_addralign;
            if (align > 1) {
                total_size = (total_size + align - 1) & ~(align - 1);
            }
            sechdrs[i].sh_addr = total_size;  
            total_size += sechdrs[i].sh_size;
        }
    }
    
    void *module_core = kmalloc(total_size);
    if (!module_core) {
        kprint_str("Module: OOM\n");
        mutex_unlock(&module_mutex);
        return -1;
    }
    memset(module_core, 0, total_size);
    
    for (int i = 0; i < hdr->e_shnum; i++) {
        if (sechdrs[i].sh_flags & SHF_ALLOC) {
            uint64_t offset = sechdrs[i].sh_addr;  
            sechdrs[i].sh_addr = (uint64_t)module_core + offset;  
            memcpy((void *)sechdrs[i].sh_addr, (uint8_t *)module_image + sechdrs[i].sh_offset, sechdrs[i].sh_size);
        } else {
             
             
            sechdrs[i].sh_addr = (uint64_t)module_image + sechdrs[i].sh_offset;
        }
    }
    
    module_t *mod = kmalloc(sizeof(module_t));
    if (!mod) {
        kfree(module_core);
        mutex_unlock(&module_mutex);
        return -1;
    }
    memset(mod, 0, sizeof(module_t));
    mod->core_layout_base = module_core;
    mod->core_layout_size = total_size;
    mod->state = MODULE_STATE_COMING;
    
    if (param_values) {
        const char *p = param_values;
        while (*p) {
            if (strncmp(p, "name=", 5) == 0) {
                const char *val = p + 5;
                int i = 0;
                while (*val && *val != ' ' && i < MODULE_NAME_LEN - 1) {
                    mod->name[i++] = *val++;
                }
                mod->name[i] = 0;
            }
             
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;
        }
    }
    if (mod->name[0] == 0) {
        strcpy(mod->name, "unnamed");
    }
    
    unsigned int symindex = 0;
    char *strtab = NULL;
    
    for (int i = 0; i < hdr->e_shnum; i++) {
        if (sechdrs[i].sh_type == SHT_SYMTAB) {
            symindex = i;
            strtab = (char *)module_image + sechdrs[sechdrs[i].sh_link].sh_offset;
            break;
        }
    }
    
    if (!symindex) {
        kprint_str("Module: No symbol table found\n");
        kfree(module_core);
        kfree(mod);
        mutex_unlock(&module_mutex);
        return -1;
    }
    
    for (int i = 0; i < hdr->e_shnum; i++) {
        if (sechdrs[i].sh_type == SHT_RELA) {
            if (apply_relocations(sechdrs, strtab, symindex, i, mod) != 0) {
                kfree(module_core);
                kfree(mod);
                mutex_unlock(&module_mutex);
                return -1;
            }
        }
    }
    
    Elf64_Sym *syms = (Elf64_Sym *)sechdrs[symindex].sh_addr;
    unsigned int num_syms = sechdrs[symindex].sh_size / sizeof(Elf64_Sym);
    
    for (unsigned int i = 0; i < num_syms; i++) {
        const char *name = strtab + syms[i].st_name;
        if (strcmp(name, "init_module") == 0) {
            if (syms[i].st_shndx != SHN_UNDEF) {
                mod->init = (module_init_t)(sechdrs[syms[i].st_shndx].sh_addr + syms[i].st_value);
            }
        } else if (strcmp(name, "cleanup_module") == 0) {
             if (syms[i].st_shndx != SHN_UNDEF) {
                mod->exit = (module_exit_t)(sechdrs[syms[i].st_shndx].sh_addr + syms[i].st_value);
            }
        }
    }
    
    if (!mod->init) {
        kprint_str("Module: init_module not found\n");
        kfree(module_core);
        kfree(mod);
        mutex_unlock(&module_mutex);
        return -1;
    }
    
    int ret = mod->init();
    if (ret != 0) {
        kprint_str("Module: init failed\n");
        kfree(module_core);
        kfree(mod);
        mutex_unlock(&module_mutex);
        return ret;
    }
    
    mod->state = MODULE_STATE_LIVE;
    list_add(&mod->list, &modules);
    
    kprint_str("Module loaded successfully.\n");
    mutex_unlock(&module_mutex);
    return 0;
}

long sys_delete_module(const char *name_user, unsigned int flags) {
    (void)flags;
    if (!module_initialized) return -1;
    
    mutex_lock(&module_mutex);
    
    struct list_head *pos, *n;
    module_t *mod;
    
    list_for_each_safe(pos, n, &modules) {
        mod = list_entry(pos, module_t, list);
        if (strcmp(mod->name, name_user) == 0) {
             if (mod->state != MODULE_STATE_LIVE) {
                kprint_str("Module busy or not live.\n");
                mutex_unlock(&module_mutex);
                return -1;
            }
            
            mod->state = MODULE_STATE_GOING;
            
            if (mod->exit) {
                mod->exit();
            }
            
            list_del(&mod->list);
            kfree(mod->core_layout_base);
            kfree(mod);
            
            kprint_str("Module unloaded: ");
            kprint_str(name_user);
            kprint_newline();
            
            mutex_unlock(&module_mutex);
            return 0;
        }
    }
    
    kprint_str("Module not found: ");
    kprint_str(name_user);
    kprint_newline();
    
    mutex_unlock(&module_mutex);
    return -1;
}

uint64_t module_kallsyms_lookup_name(const char *name) {
    return resolve_symbol(name);
}
