/*
 * kernel_module.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/device.h>       // for device classes - class_create()
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#include "ioctl.h"

#define DRVNAME KBUILD_MODNAME
#define MINOR_START 0
#define MINOR_COUNT 5
#define BUFFER_SIZE 1024
#define CLASS_NAME "my_driver_class"

MODULE_LICENSE("Dual BSD/GPL");

static int __init cdrv_init(void);
static void __exit cdrv_exit(void);

// see linux/fs.h
static int mydev_open(struct inode *inode, struct file *filep);
static int mydev_release(struct inode *inode, struct file *filep);
static ssize_t mydev_read(struct file *filep, const char __user *buff,
		size_t count, loff_t *offset);
static ssize_t mydev_write(struct file *filep, const char __user *buff,
		size_t count, loff_t *offset);
static long mydev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

// register functions
static struct file_operations mydev_fcalls = {
		.owner = THIS_MODULE,
		.open = mydev_open,
		.release = mydev_release,
		.read = mydev_read,
		.write = mydev_write,
		.unlocked_ioctl = mydev_ioctl
};

struct my_cdev {
	struct semaphore sync;
	int current_length;
	char* buffer;
	struct cdev cdev; // required for cdev of kernel
	unsigned int writing; // number open for writing
	unsigned int reading; // number open for reading
};

static dev_t dev_num; // major & minor number
static struct my_cdev* my_devs;
static struct class *my_class;

module_init(cdrv_init);
module_exit(cdrv_exit);

static int __init cdrv_init(void) {
	int result = -ENOMEM;
	int i;
	pr_info("cdrv_init()\n");

	// allocate memory for all my devices
	my_devs = kmalloc(MINOR_COUNT * sizeof(struct my_cdev), GFP_KERNEL);
	if (!my_devs) {
		pr_err("kmalloc failed!\n");
		goto err1;
	}

	result = alloc_chrdev_region(&dev_num, MINOR_START, MINOR_COUNT, DRVNAME);
	if (result < 0) {
		pr_err("alloc_chrdev_region failed\n");
		goto err2;
	}
	pr_info("major nr: %d, minor nr: %d\n", MAJOR(dev_num), MINOR(dev_num));

	my_class = class_create(THIS_MODULE, CLASS_NAME);
	if (!my_class) {
		pr_err("class_create failed\n");
		goto err2;
	}

	// init devices
	for (i = 0; i < MINOR_COUNT; i++) {
		dev_t cur_devnr = MKDEV(MAJOR(dev_num), MINOR(dev_num)+i);

		// setup fcalls for dev
		cdev_init(&my_devs[i].cdev, &mydev_fcalls);
		my_devs[i].cdev.owner = THIS_MODULE;

		// inti vars
		sema_init(&my_devs[i].sync, 1); // 1 used as mutex
		my_devs[i].current_length = 0;
		my_devs[i].buffer = NULL;
		my_devs[i].writing = 0;
		my_devs[i].reading = 0;

		result = cdev_add(&my_devs[i].cdev, cur_devnr, 1);
		if (result) {
			i--; // error in creation => device needs not be released
			goto err3;
		}
		device_create(my_class, NULL, cur_devnr, NULL, "mydev%d",
		MINOR_START + i);
		pr_info("Created device, minor %d\n",MINOR_START + i);
	}

	return 0;
	err3:
	// remove created cdevs
	for (; i >= 0; i--) {
		cdev_del(&my_devs[i].cdev);
	}
	// unregister device region
	unregister_chrdev_region(dev_num, MINOR_COUNT);
	err2: kfree(my_devs);
	err1: return result;
}

static void __exit cdrv_exit(void) {
	int i;
	pr_info("cdrv_exit()\n");
	for (i = 0; i < MINOR_COUNT; i++) {
		device_destroy(my_class, my_devs[i].cdev.dev);
		cdev_del(&my_devs[i].cdev);
		if (my_devs[i].buffer != NULL) {
			kfree(my_devs[i].buffer);
		}
		pr_info("Cleanup device, minor %d\n", i);
	}
	class_destroy(my_class);
	unregister_chrdev_region(dev_num, MINOR_COUNT);
	kfree(my_devs);
	pr_info("cdrv_exit() end\n");
}

static int mydev_open(struct inode *inode, struct file *filep) {
	struct my_cdev *dev;
	pr_info("mydev_open()\n");
	dev = container_of(inode->i_cdev, struct my_cdev, cdev); // point to device based on container_of macro
	filep->private_data = dev;
	filep->f_pos = 0; // important, position should be used in program so init = 0

	// get lock
	if (down_interruptible(&dev->sync)) {

		return -ERESTARTSYS;
	}

	// check if in write mode
	if (filep->f_mode & FMODE_WRITE) {
		// Check if device has already been opened
		if (dev->writing > 0) {
			up(&dev->sync);
			return -EBUSY;
		}
		// opened for writing
		if (dev->buffer == NULL) {
			dev->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
			if (dev->buffer == NULL) {
				up(&dev->sync);
				return -ENOMEM;
			}
		}
		dev->writing++;
	}

	// check if in read mode
	if (filep->f_mode & FMODE_READ) {
		// noone opened for writing => no buffer => error
		if(dev->buffer == NULL) {
			up(&dev->sync);
			return -ENOMEM;
		}
	}
	// release lock
	up(&dev->sync);
	pr_info("mydev_open()=0\n");
	return 0; // for success
}
static int mydev_release(struct inode *inode, struct file *filep) {
	struct my_cdev *dev;
	pr_info("mydev_release()\n");
	dev = filep->private_data;

	// get lock
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	if (dev->writing > 0) {
		dev->writing--;
	}
	if (dev->reading > 0) {
		dev->reading--;
	}

	// release lock
	up(&dev->sync);
	pr_info("mydev_release()=0\n");
	return 0; // for success
}

// return errno negative => return -ENOMEM;
static ssize_t mydev_read(struct file *filep, const char __user *buff,
		size_t count, loff_t *offset) {
	struct my_cdev *dev;
	int allowed_count;
	pr_info("mydev_read()\n");
	dev = filep->private_data;

	// get lock
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	// check read allowed
	if (*offset >= dev->current_length) {
		up(&dev->sync);
		return -ESPIPE;
	}

	allowed_count = dev->current_length - *offset;
	if (allowed_count < count) {
		count = allowed_count;
	}

	count -= copy_to_user(buff, dev->buffer + *offset, count);
	*offset += count;

	// release lock
    up(&dev->sync);
    pr_info("mydev_read()=%d\n",count);
	return count;
}

static ssize_t mydev_write(struct file *filep, const char __user *buff,
		size_t count, loff_t *offset) {
	struct my_cdev *dev;
	int allowed_count = 0;
	pr_info("mydev_write()\n");

	dev = filep->private_data;

	// get lock
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	//TODO: Buffer löschen offset erlaubt? wegen mydev_release?
	allowed_count = BUFFER_SIZE - *offset;
	if (allowed_count < count) {
		count = allowed_count;
	}

	count -= copy_from_user(dev->buffer + *offset, buff, count);
	*offset += count;

	if (dev->current_length < *offset) {
		dev->current_length = *offset;
	}

	// release lock
	up(&dev->sync);
	pr_info("mydev_write()=%d\n",count);
	return count;
}

static long mydev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
	struct my_cdev *dev;
	long retVal = 0;
	pr_info("mydev_ioctl()\n");
	dev = filep->private_data;

	if (_IOC_TYPE(cmd) != IOC_MY_TYPE) {
		pr_info("_IOC_TYPE(cmd): %x != IOC_MY_TYPE: %x",_IOC_TYPE(cmd),IOC_MY_TYPE);
		return -EACCES;
	}

	// get lock
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	if (_IOC_DIR(cmd) == _IOC_NONE) {
		switch(_IOC_NR(cmd)) {
			case IOC_NR_READCNT:
				pr_info("mydev_ioctl(): reading: %d\n", dev->reading);
				retVal = dev->reading;
				break;
			case IOC_NR_WRITECNT:
				pr_info("mydev_ioctl(): writing: %d\n", dev->writing);
				retVal = dev->writing;
				break;
			case IOC_NR_CLEAR:
				pr_info("mydev_ioctl(): clear()\n");
				dev->current_length = 0;
				filep->f_pos = 0;
				break;
			default:
				;
		}
	}
	// release lock
	up(&dev->sync);
	pr_info("mydev_ioctl()=%ld\n", retVal);
	return retVal;
}
