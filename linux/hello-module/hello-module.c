/****************************************************************************
*
*   Kernel version of hello-world, basically, a super simple module.
*
****************************************************************************/

/* ---- Include Files ---------------------------------------------------- */

#include <linux/module.h>

/* ---- Public Variables ------------------------------------------------- */
/* ---- Private Constants and Types -------------------------------------- */
/* ---- Private Variables ------------------------------------------------ */
/* ---- Private Function Prototypes -------------------------------------- */
/* ---- Functions -------------------------------------------------------- */

/****************************************************************************
*
*  hello_init
*
*     Initialization function called when the module is loaded.
*
****************************************************************************/

static int __init hello_init( void )
{
    printk( "hello_init called\n" );
	//disable interrupts
	/*asm(
			"mrs r11, cpsr \n\t"
			"orr r11, r11, #0xd3 \n\t"
			"msr cpsr_c, r11 \n\t"
	   );*/

	unsigned int addr = 0x5e000000;
	unsigned int pdbr_addr = 0xBFBFBFBF;
	asm(
			"mrc p15, 0, r1, c2, c0, 0 \n\t"
			"mov %0, r1 \n\t"
			: "=rm" (pdbr_addr)
			:
			: "memory"
	   );
	pdbr_addr &= 0xFFFFC000;
	pdbr_addr = 0xC0004000;

	printk("Addr: %08x, current stack: %p\n",pdbr_addr,&addr);
	
	unsigned int* pdbr = (unsigned int*) pdbr_addr;
	int i;
	for (i=0;i<4096;i++){
		if (pdbr[i]!=0) 
		printk("%d: %08x\n",i,pdbr[i]);
	}



	//enable interrupts
	/*asm(
			"mrs r11,cpsr \n\t"
			"bic r11,r11,#0xc0 \n\t"
			"msr cpsr_c,r11\n\t"
	   );*/

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

