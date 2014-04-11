#include <linux/module.h>
#include <linux/slab.h>


/****************************************************************************
*
*  hello_init
*
*     Initialization function called when the module is loaded.
*
****************************************************************************/

unsigned int pdbr_addr = 0xBFBFBFBF;

/*will access a megabyte of memory*/
void access(unsigned int ptr){
	//unsigned char* data = (unsigned char*) ptr;
	unsigned char* data = (unsigned char*) 0xd0000000;
	unsigned int index = ((unsigned int)data) >> 20;
	volatile unsigned int* pdbr = (unsigned int*) pdbr_addr;
	int i;
	unsigned int back;
	unsigned char* tmp;
	tmp = kmalloc(1024*1024, GFP_ATOMIC);
	printk("pdbr[%d]=%08x\n",index,pdbr[index]);
	back = pdbr[index];
	/*if (pdbr[index]!=0){
		printk("Access: Entry already exists: %08x, quit\n",pdbr[index]);
		return;
	}*/
	pdbr[index]= (ptr & 0xFFF00000) | 0x040E;
	printk("pdbr[%d]=%08x\n",index,pdbr[index]);
	//flush tlb, not working
	asm volatile(
			"mov r4, #0 \n\t"
			"mcr p15, 0, r4, c8, c6, 0 \n\t"
			:
			:
			: "r4"
			);
	memcpy(tmp, data, 1024*1024);
	printk("pdbr[%d]=%08x\n",index,pdbr[index]);
	pdbr[index]=back;

	asm volatile(
			"mov r4, #0 \n\t"
			"mcr p15, 0, r4, c8, c6, 0 \n\t"
			:
			:
			: "r4"
			);
	for (i=0;i<1024*1024;i++){
		if (i%(1024*16) == 0) printk("%02x ",tmp[i]);
	}
	printk("\n");
	kfree(tmp);

}


static int __init hello_init( void )
{
	int i;
	unsigned int tmp;
    printk( "hello_init called\n" );
	
	//try to fetch pdbr
	asm volatile(
			"mrc p15, 0, %0, c2, c0, 0 \n\t"
			: "=rm" (tmp)
			:
			: "memory"
			);
	pdbr_addr = 0x40000000 + (tmp & 0xFFFFC000);
	
	printk("got pdbr %08x\n",pdbr_addr);

	//get current mode
	asm volatile(
			"mrs %0, cpsr"
			: "=rm" (tmp)
			:
			: "memory"
			);

	//We should be in Supervisor mode
	if ((tmp & 0x1F) != 0x13 ){
		printk("We are not in the supervisor mode!\n");
		return -1;
	}

	
	//close interrupt
	asm volatile(
			"mrs r4, cpsr \n\t"
			"orr r4, r4, #0xc0 \n\t"
			"msr cpsr, r4"
			:
			: 
			: "r4"
			);

	//interrupt should be closed now
	access(0x80000000);


	asm volatile(
			"mrs r4, cpsr \n\t"
			"bic r4, r4, #0xc0 \n\t"
			"msr cpsr, r4"
			:
			: 
			: "r4"
			);
	for (i=0;i<1024*1024;i++){
		if (i%(1024*16) == 0) printk("%02x ",((unsigned char*)0xC0000000)[i]);
	}
	printk("\n");

	printk("Done.\n");

    return -1;

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

