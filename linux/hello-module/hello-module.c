#include <linux/module.h>
#include <linux/slab.h>
//#include <pt.h>

//#include "cache_headers.h"
#include <asm/tlbflush.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
//#define __NO_VERSION__
//#include <linux/version.h>
#include "pt.h"

void swatt_arm_set_ttbr0(uint32_t value)
{
	asm volatile(
			"mcr	p15, 0, %0, c2, c0, 0\n\t"
			//"mov	pc, lr\n\t"
			:
			: "r"(value)
			);
        return;
}

uint32_t swatt_arm_read_ttbr0(void)
{
  uint32_t value;
  //	mrc	p15, 0, r0, c2, c0, 0
  //	mov	pc, lr
	asm volatile(
			"mrc	p15, 0, %0, c2, c0, 0\n\t"
			//"mov	pc, lr\n\t"
			: "=r"(value)
			);

	return value;

}	

void swatt_arm_set_ttbr1(uint32_t value)
{
  //	mcr	p15, 0, r0, c2, c0, 1
  //	mov	pc, lr
	asm volatile(
			"mcr	p15, 0, %0, c2, c0, 1\n\t"
			//"mov	pc, lr\n\t"
			:
			: "r"(value)
			);
	return;

}	

uint32_t swatt_arm_read_ttbr1(void)
{
  //	mrc	p15, 0, r0, c2, c0, 1
  //	mov	pc, lr
  uint32_t value;
	asm volatile(
			"mrc	p15, 0, %0, c2, c0, 1\n\t"
			//"mov	pc, lr\n\t"
			: "=r"(value)
			);
	return value;
}


void swatt_arm_set_ttbcr(uint32_t value)
{
  //	mcr	p15, 0, r0, c2, c0, 2
  //	mov	pc, lr
	asm volatile(
			"mcr	p15, 0, %0, c2, c0, 2\n\t"
			//"mov	pc, lr\n\t"
			:
			: "r"(value)
			);
	return;

}


uint32_t swatt_arm_get_ttbcr(void)
{
  //	mrc	p15, 0, r0, c2, c0, 2
  //	mov	pc, lr
  uint32_t value;
	asm volatile(
			"mrc	p15, 0, %0, c2, c0, 2\n\t"
			//"mov	pc, lr\n\t"
			: "=r"(value)
			);
	return value;
}

static uint32_t origin_ttbcr;
static uint32_t origin_ttbr0;
static uint32_t origin_ttbr1;
static uint32_t l1_pt[ARM_L1_PAGETABLE_ENTRIES] __attribute__(( aligned(16384),section(".data") ));

void backup_os_pt(void)
{
  	origin_ttbcr = swatt_arm_get_ttbcr();
        printk("origin ttbcr is %#x\n", origin_ttbcr);
       	origin_ttbr0 = swatt_arm_read_ttbr0();
        printk("origin ttbro is %#x\n", origin_ttbr0);
       	origin_ttbr1 = swatt_arm_read_ttbr1();
        printk("origin ttbr1 is %#x\n", origin_ttbr1);
        printk("backup os pt is done\n");
}

void restore_os_pt(void)
{
	swatt_arm_set_ttbcr(origin_ttbcr);
	swatt_arm_set_ttbr0(origin_ttbr0);
	swatt_arm_set_ttbr1(origin_ttbr1);
}

static void mm_create1M_mapping(vaddr_t vaddr, paddr_t paddr, uint32_t s_ns_access)
{
	uint32_t idx = 0;
	arm_vmsa_lvl1desc_section_t* l1_pte;
	
	idx = (vaddr >> 20) & 0xFFF;
	l1_pte = (arm_vmsa_lvl1desc_section_t*)&l1_pt[idx];

	l1_pte->fields.type = ARM_VMSA_LVL1DESC_TYPE_SECTION;
	l1_pte->fields.address = (paddr >> 20);
	l1_pte->fields.ns = s_ns_access;
	l1_pte->fields.notglobal = 0;
	
	l1_pte->fields.accesspermission = ARM_VMSA_ACCESS_FULL;  
	l1_pte->fields.executenever = ARM_VMSA_EXECUTE; 
}

static uint32_t* mm_get_lv1_entry(uint32_t ttbr0, vaddr_t vaddr)
{
    return &(((uint32_t*)ttbr0)[vaddr >> 20]);
}

static vaddr_t mm_alloc_1k_aligned(void)
{
	vaddr_t addr = (vaddr_t)kmalloc(1024*2, GFP_KERNEL);
	return addr & 0xFFFFFC00;
}

static void mm_create4K_mapping(vaddr_t vaddr, paddr_t paddr, uint32_t s_ns_access)
{
    arm_vmsa_lvl1desc_pagetable_t* p_l1_pte;
	arm_vmsa_lvl2desc_page4k_t* p_l2_pte;

    // Get l1 pte
    p_l1_pte = (arm_vmsa_lvl1desc_pagetable_t*)mm_get_lv1_entry((uint32_t)l1_pt, vaddr);
	if(p_l1_pte->fields.type == ARM_VMSA_LVL1DESC_TYPE_SECTION || p_l1_pte->fields.address == 0)
        p_l1_pte->bytes = 0;
    
    // Fill l1 pte with l2 page info, set l2 pte
    if(p_l1_pte->bytes == 0) // No L2 PTE
    {
        uint32_t* l2_page;
        paddr_t l2_page_paddr;

		// [TODO] Memory leak here.
		// Alloc 1K aligned 
        l2_page = (uint32_t*)mm_alloc_1k_aligned(); // we only need 1KB with 1KB alignment
        //ASSERT( ((uint32_t)l2_page & 0xFFFFFC00) );
        
        
        p_l1_pte->fields.type = ARM_VMSA_LVL1DESC_TYPE_PAGETABLE;
        p_l1_pte->fields.ns = s_ns_access;       //Secure world Access?
        l2_page_paddr = virt_to_phys(l2_page);
        p_l1_pte->fields.address = ((uint32_t)l2_page_paddr) >> ARM_VMSA_L2_PAGE_SHIFT;
		
        //MEMDEBUG("[mm_srv_create_mapping] Cannot find the L2 PTE, Now Update L1 PTE to be 0x%X\n", p_l1_pte->bytes);
        
    }
	
    {
        paddr_t l2_page_paddr;
        uint32_t* l2_page;
        uint32_t l2_offset = 0;
        
        l2_offset = (vaddr >> 12) & 0xFF;
        
        l2_page_paddr = (p_l1_pte->fields.address) << ARM_VMSA_L2_PAGE_SHIFT;
        l2_page = (uint32_t*)(phys_to_virt(l2_page_paddr));
        p_l2_pte = (arm_vmsa_lvl2desc_page4k_t*)&l2_page[l2_offset];
    
        p_l2_pte->bytes = 0;
        p_l2_pte->fields.type = ARM_VMSA_LVL2DESC_TYPE_PAGE4K;
        p_l2_pte->fields.address = paddr_to_l2pfn(paddr);
        p_l2_pte->fields.c=1;p_l2_pte->fields.b=1;p_l2_pte->fields.tex=0x5;	//WB-WA cacheable

        p_l2_pte->fields.notglobal = 0;

		p_l2_pte->fields.accesspermission = ARM_VMSA_ACCESS_FULL;  
		p_l2_pte->fields.execute_never = ARM_VMSA_EXECUTE; 
	
        /*if (permission & MEM_WRITE)
        {
			if (permission & MEM_READ)
            	p_l2_pte->fields.accesspermission = ARM_VMSA_ACCESS_FULL;  
        }
        else if (permission & MEM_READ)
        {
            	p_l2_pte->fields.accesspermission = ARM_VMSA_ACCESS_USER_READ; 
        }
        else
        {
            p_l2_pte->fields.accesspermission = ARM_VMSA_ACCESS_KERN_ONLY;
			// [TODO] Should kernel has the ability to execute this code?
        }
        
        if(permission & MEM_EXECUTE)
        {
            p_l2_pte->fields.execute_never = ARM_VMSA_EXECUTE;  
            p_l2_pte->fields.accesspermission = ARM_VMSA_ACCESS_USER_READ;
        }
        else
        {
            p_l2_pte->fields.execute_never = ARM_VMSA_NONEXECUTE;
        }*/
    }
	
	//flush_cache_all();
	//MEMDEBUG("[mm_srv_create_mapping] The L2 PTE is 0x%X\n", p_l2_pte->bytes);
    //PRINT("[MMU] mm_srv_create_mapping, vaddr:0x%X, paddr:0x%X, l1_pte:0x%X, l2_pte:0x%X\n", vaddr, paddr, (uint32_t)p_l1_pte, (uint32_t)p_l2_pte);
}


void mm_create_mapping(vaddr_t vaddr, paddr_t paddr, int create_option)
{
	if(create_option == CREATE_1M_PAGE)
		mm_create1M_mapping(vaddr, paddr, ARM_VMSA_NS_NONSECURE);
	else if(create_option == CREATE_4K_PAGE)
		mm_create4K_mapping(vaddr, paddr, ARM_VMSA_NS_NONSECURE);
}

void load_new_pt(void)
{
	
        //	flush_cache_all();
	swatt_arm_set_ttbr0(virt_to_phys(&l1_pt));
	flush_tlb_all();
	
	//invalidate caches
    //arm_invalidate_caches();

    //invalidate tlb
    //arm_invalidate_tlb();
}

/****************************************************************************
*
*  hello_init
*
*     Initialization function called when the module is loaded.
*
****************************************************************************/

unsigned int pdbr_addr = 0xBFBFBFBF;



static int __init hello_init( void )
{
    int i;
    unsigned int tmp;
    printk( "hello_init called\n" );
    backup_os_pt();
	
    printk("\n");

    printk("Well done.\n");

    return 0;

} // hello_init

/****************************************************************************
*
*  hello_exit
*
*       Called to perform module cleanup when the module is unloaded.
*
****************************************************************************/

static void __exit hello_exit( void )
{
    printk( "hello_exit called\n" );

} // hello_exit

/****************************************************************************/

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Dave Hylands");
MODULE_DESCRIPTION("Hello World Module");
MODULE_LICENSE("GPL");

