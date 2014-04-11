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

	unsigned int addr = 0x5e000000;
	unsigned int pdbr_addr = 0xBFBFBFBF;
	/*pdbr should be placed here*/
	unsigned int logic_addr = 0xC0004000;

	printk("current stack: %p\n", &addr);
	unsigned int tmp = 0;
	
	/*A self test: We found 0xBF000000 always has a page_table present*/
	unsigned int* l_array = (unsigned int*) logic_addr;
	int i;
	for (i=0;i<4096;i++){
		if ((l_array[i]&0x3)!=0) {
			if (tmp==0) {
				tmp=l_array[i];
				printk("Found first nonempty pde %d, == 3056 (0xBF000000)??\n",i);
			}
			//printk("%d: %08x\n",i,l_array[i]);
		}
	}

	/*parsing that pagetable, we should found the physical addr can be
	 * accessd from another logical addr*/
	logic_addr = 0x40000000 + (tmp & 0xFFFFFC00);
	printk("page_table: %08x\n",logic_addr);
	tmp=0;
	l_array = (unsigned int*) logic_addr;
	for (i=0;i<256;i++){
		if ((l_array[i]&0x3)!=0){
			if (tmp==0) {
				tmp=l_array[i];
				printk("Found first nonempty pte %d, == 0 ??\n",i);
			}
			//printk("%d: %08x\n",i,l_array[i]);
		}
	}


	logic_addr =0x40000000 + (tmp & 0xFFFFF000);
	/*This is another logic entry to this physical address*/
	printk("Logical Addr: %08x\n",logic_addr);
	char* a = (char*)logic_addr;
	char* b = (char*)0xbf000000;

	for (i=0;i<4096;i++){
		if (a[i]!=b[i]){
			printk("Mismatch at %d\n",i);
			break;
		}
	}

	//disable interrupts
	/*asm(
			"mrs r11, cpsr \n\t"
			"orr r11, r11, #0xd3 \n\t"
			"msr cpsr_c, r11 \n\t"
	   );*/

	asm(
			"mrc p15, 0, r1, c2, c0, 0 \n\t"
			"mov %0, r1 \n\t"
			: "=rm" (pdbr_addr)
			:
			: "memory"
	   );

	//enable interrupts
	/*asm(
			"mrs r11,cpsr \n\t"
			"bic r11,r11,#0xc0 \n\t"
			"msr cpsr_c,r11\n\t"
	   );*/
	printk("Done.\n");

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

