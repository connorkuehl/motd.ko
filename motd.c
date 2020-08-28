#include "motd.h"

#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/spinlock.h>

static int motd_major = 0;
module_param(motd_major, int, S_IRUGO);

static struct motd_dev {
	char *motd;
	size_t len;

	rwlock_t lock;
} motd_dev = { 0 };

static int __init motd_init(void)
{
	int result;
	dev_t dev;

	if (motd_major) {
		dev = MKDEV(motd_major, MOTD_MINOR);
		result = register_chrdev_region(dev, MOTD_NDEVS, MOTD_NAME);
	} else {
		result = alloc_chrdev_region(&dev, MOTD_MINOR, MOTD_NDEVS, MOTD_NAME);
		motd_major = MAJOR(dev);
	}

	if (result < 0) {
		printk(KERN_WARNING MOTD_NAME ": can't get major %d\n", motd_major);
		goto out;
	}

	rwlock_init(&motd_dev.lock);

out:
	return result;
}

static void __exit motd_exit(void)
{
	dev_t dev = MKDEV(motd_major, MOTD_MINOR);

	unregister_chrdev_region(dev, MOTD_NDEVS);
}

MODULE_AUTHOR("Connor Kuehl <cipkuehl@gmail.com>");
MODULE_LICENSE("GPL");

module_init(motd_init);
module_exit(motd_exit);
