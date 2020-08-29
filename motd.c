#include "motd.h"

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
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

static void motd_trunc(struct motd_dev *dev)
{
	write_lock(&dev->lock);

	kfree(dev->motd);
	dev->motd = NULL;
	dev->len = 0;

	write_unlock(&dev->lock);
}

static int motd_open(struct inode *inode, struct file *filp)
{
	struct motd_dev *dev;

	/* Not really necessary to stash the device since we just have
	 * the one, but this will help the effort for when/if support
	 * for multiple motd devices is added. */
	dev = container_of(inode->i_cdev, struct motd_dev, cdev);
	filp->private_data = dev;

	if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
		motd_trunc(dev);

	return 0;
}

static int motd_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static loff_t motd_llseek(struct file *filp, loff_t off, int whence)
{
	struct motd_dev *dev = filp->private_data;
	loff_t new_pos;
	loff_t ret = -EINVAL;

	read_lock(&dev->lock);

	switch (whence) {
	case SEEK_SET:
		new_pos = off;
		break;
	case SEEK_CUR:
		new_pos = filp->f_pos + off;
		break;
	case SEEK_END:
		new_pos = dev->len;
		break;
	default:
		goto out;
	}

	if (new_pos < 0)
		goto out;

	filp->f_pos = new_pos;
	ret = new_pos;
out:
	read_unlock(&dev->lock);
	return ret;
}

static ssize_t motd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct motd_dev *dev = filp->private_data;
	ssize_t ret = 0;

	read_lock(&dev->lock);

	if (*f_pos >= dev->len)
		goto out;
	if (*f_pos + count > dev->len)
		count = dev->len - *f_pos;
	if(copy_to_user(buf, dev->motd + *f_pos, count)) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += count;
	ret = count;
out:
	read_unlock(&dev->lock);
	return ret;
}

static ssize_t motd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct motd_dev *dev = filp->private_data;
	char *new_data = NULL;
	ssize_t ret = 0;

	write_lock(&dev->lock);

	new_data = kmalloc(count * sizeof(*new_data), GFP_KERNEL);
	if (!new_data) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(new_data, buf, count)) {
		ret = -EFAULT;
		goto free_new_data;
	}

	// This write requires expanding the buffer.
	if (*f_pos + count > dev->len) {
		size_t expand_by = count - (dev->len - *f_pos);
		size_t new_size = dev->len + expand_by;

		char *bigger_motd = kmalloc(new_size * sizeof(*bigger_motd), GFP_KERNEL);
		if (!bigger_motd) {
			ret = -ENOMEM;
			goto free_new_data;
		}

		strncpy(bigger_motd, dev->motd, dev->len);
		kfree(dev->motd);
		dev->motd = bigger_motd;
	}

	strncpy(dev->motd + *f_pos, new_data, count);
	dev->len += count;
	*f_pos += count;
	ret = count;

free_new_data:
	kfree(new_data);
out:
	write_unlock(&dev->lock);
	return ret;
}

static struct file_operations motd_fops = {
	.owner = THIS_MODULE,
	.open = motd_open,
	.release = motd_release,
	.llseek = motd_llseek,
	.read = motd_read,
	.write = motd_write,
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

	motd_trunc(&motd_dev);

	cdev_del(&motd_dev.cdev);
	unregister_chrdev_region(dev, MOTD_NDEVS);
}

MODULE_AUTHOR("Connor Kuehl <cipkuehl@gmail.com>");
MODULE_LICENSE("GPL");

module_init(motd_init);
module_exit(motd_exit);
