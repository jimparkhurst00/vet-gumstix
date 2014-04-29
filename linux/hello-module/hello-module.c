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



static int __init hello_init( void )
{
	int i;
	unsigned int tmp;
    printk( "hello_init called\n" );
	
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

