#ifndef _A8_MMU
#define _A8_MMU

/* arm 7-a/r core System Control Register (CR) 
 * bit definitions (B3.12.17, page B3-96)
 */
#define ARM_C1_CR_M		0x0001  /* mmu */
#define ARM_C1_CR_A     0x0002  /* alignment */
#define ARM_C1_CR_C		0x0004  /* (d) cache */
//#define ARM_C1_CR_W		0x0008	/* write buffer */
//#define ARM_C1_CR_B     0x0080  /* endianness */
#define ARM_C1_CR_Z     0x0800  /* branch prediction */
#define ARM_C1_CR_I     0x1000  /* (i) cache */
#define ARM_C1_CR_V			0x2000	/*vector base address*/
//#define ARM_C1_CR_VE		0x1000000 /*vectored interrupt enable*/
//#define ARM_C1_CR_XP		0x800000 /* ARMv6 VMSA selection, this is always 1 on ARMv7 */

#endif
