#include "motd.h"

#include <linux/cdev.h>
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

	struct cdev cdev;
	rwlock_t lock;
} motd_dev = { 0 };

static int motd_open(struct inode *inode, struct file *filp)
{
	struct motd_dev *dev;

	/* Not really necessary to stash the device since we just have
	 * the one, but this will help the effort for when/if support
	 * for multiple motd devices is added. */
	dev = container_of(inode->i_cdev, struct motd_dev, cdev);
	filp->private_data = dev;

	return 0;
}

static int motd_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations motd_fops = {
	.owner = THIS_MODULE,
	.open = motd_open,
	.release = motd_release,
};

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

	cdev_init(&motd_dev.cdev, &motd_fops);
	motd_dev.cdev.owner = THIS_MODULE;

	result = cdev_add(&motd_dev.cdev, dev, 1);
	if (result) {
		printk(KERN_NOTICE MOTD_NAME ": error adding cdev\n");
		unregister_chrdev_region(dev, MOTD_NDEVS);
		goto out;
	}

out:
	return result;
}

static void __exit motd_exit(void)
{
	dev_t dev = MKDEV(motd_major, MOTD_MINOR);

	cdev_del(&motd_dev.cdev);
	unregister_chrdev_region(dev, MOTD_NDEVS);
}

MODULE_AUTHOR("Connor Kuehl <cipkuehl@gmail.com>");
MODULE_LICENSE("GPL");

module_init(motd_init);
module_exit(motd_exit);
