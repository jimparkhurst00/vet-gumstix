#ifndef _ARM_CACHE_H
#define _ARM_CACHE_H


extern void flush_cache_all(void);

extern void flush_tlb_all(void);

extern void clean_invalidate_dcache(void);

extern void arm_enable_icache(void);
extern void arm_enable_dcache(void);
extern void arm_invalidate_caches(void);
extern void arm_invalidate_tlb(void);
#endif // _ARM_CACHE_H
