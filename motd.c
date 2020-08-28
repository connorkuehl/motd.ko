#include <linux/module.h>

static int __init motd_init(void)
{
	return 0;
}

static void __exit motd_exit(void)
{
}

MODULE_AUTHOR("Connor Kuehl <cipkuehl@gmail.com>");
MODULE_LICENSE("GPL");

module_init(motd_init);
module_exit(motd_exit);
