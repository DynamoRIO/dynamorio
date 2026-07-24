#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");

static int __init
dynamorio_module_init(void)
{
    pr_info("DynamoRIO module started\n");
    return 0;
}

static void __exit
dynamorio_module_exit(void)
{
    pr_info("DynamoRIO module exited\n");
}

module_init(dynamorio_module_init);
module_exit(dynamorio_module_exit);
