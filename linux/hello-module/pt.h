#ifndef PAGETABLE_H
#define PAGETABLE_H

#include "pt-defs.h"

// Create_option
#define CREATE_1M_PAGE 	0
#define CREATE_4K_PAGE	1

extern void mm_create_mapping(vaddr_t vaddr, paddr_t paddr, int create_option);

extern void backup_os_pt(void);

extern void restore_os_pt(void);

extern void load_new_pt(void);
#endif // PAGETABLE_H