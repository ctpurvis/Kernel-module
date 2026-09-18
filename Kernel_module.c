#include <linux/kernel.h>
#include <linux/module.h>

static int __init wordswap_init(void)
{	 pr_info("wordswap: module loaded\n");
         return 0;
}

static void __exit wordswap_exit(void)
{
	pr_info("wordswap: module unloaded\n");

}

module_init(wordswap_init);
module_exit(wordswap_exit);

MODULE_LICENSE ("GPL");


