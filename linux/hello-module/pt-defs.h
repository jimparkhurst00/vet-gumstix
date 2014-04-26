#ifndef PT_DEF_H
#define PT_DEF_H

typedef unsigned int 	uint32_t;
typedef uint32_t 		vaddr_t;
typedef uint32_t 		paddr_t;
typedef uint32_t 		l2pfn_t; //l2 4K page table pfn 

#define paddr_to_l2pfn(paddr)           (paddr >> ARM_VMSA_PAGE_SHIFT)

// Refer to ARM Architecture Reference Manual ARMv7-A and ARMv7-R edition
// For TTBCR Configuration
#define ARM_VMSA_TTBCR_PD1_ENABLE       0
#define ARM_VMSA_TTBCR_PD1_DISABLE      0x20
#define ARM_VMSA_TTBCR_PD0_ENABLE       0
#define ARM_VMSA_TTBCR_PD0_DISABLE      0x10

#define ARM_L1_SECTION_ENTRIES          ( 1 << 12 )
#define ARM_L1_PAGETABLE_ENTRIES        ( 1 << 12 )
#define ARM_L2_PAGETABLE_ENTRIES        ( 1 << 8 )

#define MEM_NONE			0x0
#define MEM_READ			0x1
#define MEM_WRITE			0x10
#define MEM_EXECUTE			0x100

#define ARM_VMSA_L1PT_FLAG_MASK		0x3FF
#define ARM_VMSA_L1PT_PFN_MASK		(~ARM_VMSA_L1PT_FLAG_MASK)	
#define ARM_VMSA_L2PT_FLAG_MASK		0xFFF	
#define ARM_VMSA_L2PT_PFN_MASK		(~ARM_VMSA_L2PT_FLAG_MASK)

#define ARM_VMSA_VADDR_PAGEINDEX_MASK		0xFFF
#define ARM_VMSA_VADDR_SECTIONINDEX_MASK		0xFFFFF

#define ARM_VMSA_L1_PAGE_SHIFT		20
#define ARM_VMSA_L2_PAGE_SHIFT		10
#define ARM_VMSA_PAGE_SHIFT			12


//VMSA NS permissions
#define	ARM_VMSA_NS_NONSECURE       0x1
#define	ARM_VMSA_NS_SECURE          0x0

//VMSA Execute Never
#define ARM_VMSA_EXECUTE			0x0
#define ARM_VMSA_NONEXECUTE			0x1

//VMSA User/Kernel access permissions
#define	ARM_VMSA_ACCESS_NOACCESS            0x0
#define	ARM_VMSA_ACCESS_KERN_ONLY           0x1
#define	ARM_VMSA_ACCESS_USER_READ           0x2
#define	ARM_VMSA_ACCESS_FULL                0x3

//VMSA domain access permissions
#define	ARM_VMSA_DOMAIN_NOACCESS	0x0
#define	ARM_VMSA_DOMAIN_CLIENT		0x1
#define ARM_VMSA_DOMAIN_RESERVED	0x2
#define	ARM_VMSA_DOMAIN_MANAGER		0x3


//VMSA level-1 descriptor types
#define ARM_VMSA_LVL1DESC_TYPE_INVALID		0x0
#define ARM_VMSA_LVL1DESC_TYPE_PAGETABLE	0x1
#define ARM_VMSA_LVL1DESC_TYPE_SECTION		0x2
#define	ARM_VMSA_LVL1DESC_TYPE_RESERVED		0x3

//VMSA level-2 descriptor types
#define ARM_VMSA_LVL2DESC_TYPE_PAGE4K			0x1


//VMSA TTBR region cacheability attributes
#define ARM_VMSA_TTBR_REGIONCACHE_NONCACHEABLE				0x0
#define ARM_VMSA_TTBR_REGIONCACHE_WBWA								0x1	//write-back, write-allocate
#define ARM_VMSA_TTBR_REGIONCACHE_WT									0x2	//write-through
#define ARM_VMSA_TTBR_REGIONCACHE_WBNOWA							0x3 //write-back but no write-allocate

//VMSA CONTEXTIDR constants for our hypervisor
#define	VMSA_CONTEXTIDR_ASID_HYPERPHONE		0xFF						//we choose 0xFF as our ASID
#define VMSA_CONTEXTIDR_PROCID_HYPERPHONE	0xDC0DE					//our unique process id for debug/trace logic


//VMSA page and section sizes
#define ARM_VMSA_SECTIONSIZE_1M             (1UL << 20)
#define ARM_VMSA_L1PAGESIZE_1M              (1UL << 20)
#define ARM_VMSA_PAGESIZE_4K                (1UL << 12)
#define PAGE_SIZE_4K                        (ARM_VMSA_PAGESIZE_4K)


//level-1 section descriptor
typedef union arm_vmsa_lvl1desc_section {
    uint32_t bytes;
    struct
  {
    uint32_t type:2;	//should be ARM_VMSA_LVL1DESC_TYPE_SECTION
    uint32_t b:1; // Bufferable
        uint32_t c:1;	//these form part of the caching policy for the section (B3.7.2, ARM DDI 0406B) 
        uint32_t executenever:1;	//execute-never bit, if set to 1 no executions are permitted in the section
    uint32_t domain:4;	//domain to which this entry belongs (0-15)
        uint32_t imp:1;	//the meaning of this is implementation defined
        uint32_t accesspermission:2;		//access permission bits 0 and 1 (B3.6.1, ARM DDI 0406B)
        uint32_t tex:3;	//these form part of the caching policy for the section (B3.7.2, ARM DDI 0406B)
        uint32_t accesspermission_x:1;	//access permission bit 2 (B3.6.1, ARM DDI 0406B)
        uint32_t shareable:1;	//determines if this translation is for shareable memory (B3.7.2, ARM DDI 0406B)
        uint32_t notglobal:1;	//if set the section in the TLB is non-global
        uint32_t supersection:1;	// always 0 for a section 
        uint32_t ns:1; //if set, specifies that this address is for NS world, but needs support from bridge h/w	
        uint32_t address:12;	//1MB aligned section base address
    }__attribute__ ((packed)) fields;
} __attribute__ ((packed)) arm_vmsa_lvl1desc_section_t;


//VMSA level-2 (second level) descriptor formats
//see B3.3.1, ARM DDI 0406B
typedef union arm_vmsa_lvl2desc_page4k {
    uint32_t bytes;
    struct
  {
    uint32_t execute_never:1;	//if 1 then there is a prefetch abort (similar to NX)
    uint32_t type:1;						//should be ARM_VMSA_LVL2DESC_TYPE_PAGE4K
        uint32_t b:1; //
        uint32_t c:1;	//these form part of the caching policy for the section (B3.7.2, ARM DDI 0406B) 
        uint32_t accesspermission:2;		//access permission bits 0 and 1 (B3.6.1, ARM DDI 0406B)
        uint32_t tex:3;	//these form part of the caching policy for the section (B3.7.2, ARM DDI 0406B)
        uint32_t accesspermission_x:1;	//access permission bit 2 (B3.6.1, ARM DDI 0406B)
        uint32_t shareable:1;	//determines if this translation is for shareable memory (B3.7.2, ARM DDI 0406B)
        uint32_t notglobal:1;	//if set the section in the TLB is non-global
        l2pfn_t address:20;	//4K aligned physical memory page address
    }__attribute__ ((packed)) fields;
} __attribute__ ((packed)) arm_vmsa_lvl2desc_page4k_t;

//level-1 page-table desriptor
typedef union arm_vmsa_lvl1desc_pagetable {
    uint32_t bytes;
    struct
  {
    uint32_t type:2;	//should be ARM_VMSA_LVL1DESC_TYPE_PAGETABLE
    uint32_t sbz0:1;	//reserved set to 0
    uint32_t ns:1;	//if set, specifies that this address is for NS world, but needs support from bridge h/w
    uint32_t sbz1:1;	//reserved set to 0
    uint32_t domain:4;	//domain to which this entry belongs (0-15)
        uint32_t imp:1;	//the meaning of this is implementation defined
        uint32_t address:22;	//1K aligned page-table base address
    }__attribute__ ((packed)) fields;
} __attribute__ ((packed)) arm_vmsa_lvl1desc_pagetable_t;

#endif // PT_DEF_H