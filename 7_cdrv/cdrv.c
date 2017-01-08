/*
 * kernel_module.c
 *
 *  Created on: Dec 19, 2016
 *
 * Authors:
 * is141315 - Neumair Florian
 * is141305 - Gimpl Thomas
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
#define PROC_FOLDER "is141315"
#define PROC_FILE "info"

MODULE_LICENSE("Dual BSD/GPL");

static int __init cdrv_init(void);
static void __exit cdrv_exit(void);

// see linux/fs.h
static int mydev_open(struct inode *inode, struct file *filep);
static int mydev_release(struct inode *inode, struct file *filep);
static ssize_t mydev_read(struct file *filep, const char __user *buff, size_t count, loff_t *offset);
static ssize_t mydev_write(struct file *filep, const char __user *buff, size_t count, loff_t *offset);
static long mydev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

// proc stuff
static int my_seq_file_open(struct inode *inode, struct file *filep);
static void *my_seq_file_start(struct seq_file *sf, loff_t *pos);
static void my_seq_file_stop(struct seq_file *sf, void *it);
static void *my_seq_file_next(struct seq_file *sf, void *it, loff_t *pos);
static int my_seq_file_show(struct seq_file *sf, void *it);

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
	struct semaphore sync; // sync
	int buffer_size; // length buffer should have
	int current_length; // current length
	char* buffer; // data
	struct cdev cdev; // required for cdev of kernel
	unsigned int writing; // number open for writing
	unsigned int reading; // number open for reading
	unsigned int sum_open; // total open count
	unsigned int sum_read; // total read count
	unsigned int sum_write; // total write count
	unsigned int sum_release; // total release count
	unsigned int sum_clear; // total clear count
	wait_queue_head_t wq_read; // wait queue for reads
	wait_queue_head_t wq_write; // wait queue for writes
};

// Proc syscalls
static struct file_operations my_proc_fcalls = {
    .owner = THIS_MODULE,
    .open = my_seq_file_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release
};

// Proc File functions
static struct seq_operations my_seq_options = {
    .start = my_seq_file_start,
    .next = my_seq_file_next,
    .stop = my_seq_file_stop,
    .show = my_seq_file_show
};

static dev_t dev_num; // major & minor number
static struct my_cdev* my_devs;
static struct class *my_class;

static struct proc_dir_entry *proc_parent; // -> /proc/is1413xx
static struct proc_dir_entry *proc_entry; // -> /proc/is1413xx/info

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

	// Create /proc
	proc_parent = proc_mkdir(PROC_FOLDER, NULL);
	if (proc_parent == NULL) {
		pr_err("proc_mkdir(): proc creation failed\n");
		result = -EIO;
		goto err3;
	}
	proc_entry = proc_create_data(PROC_FILE, 0444, proc_parent, &my_proc_fcalls, NULL);
	if (proc_entry == NULL) {
		pr_err("proc_create_data(): proc creation failed\n");
		result = -EIO;
		goto err4;
	}

	// init devices
	for (i = 0; i < MINOR_COUNT; i++) {
		dev_t cur_devnr = MKDEV(MAJOR(dev_num), MINOR(dev_num)+i);

		// setup fcalls for dev
		cdev_init(&my_devs[i].cdev, &mydev_fcalls);
		my_devs[i].cdev.owner = THIS_MODULE;

		// init vars
		sema_init(&my_devs[i].sync, 1); // 1 used as mutex
		my_devs[i].buffer_size = BUFFER_SIZE;
		my_devs[i].current_length = 0;
		my_devs[i].buffer = NULL;
		my_devs[i].writing = 0;
		my_devs[i].reading = 0;
		my_devs[i].sum_open = 0;
		my_devs[i].sum_read = 0;
		my_devs[i].sum_write = 0;
		my_devs[i].sum_release = 0;
		my_devs[i].sum_clear = 0;
		// init queues
		init_waitqueue_head(&my_devs[i].wq_read);
		init_waitqueue_head(&my_devs[i].wq_write);

		result = cdev_add(&my_devs[i].cdev, cur_devnr, 1);
		if (result) {
			i--; // error in creation => device needs not be released
			goto err5;
		}
		device_create(my_class, NULL, cur_devnr, NULL, "mydev%d",
		MINOR_START + i);
		pr_info("Created device, minor %d\n",MINOR_START + i);
	}

	return 0;
	err5:
		remove_proc_entry(PROC_FILE, proc_parent);
	err4:
		remove_proc_entry(PROC_FOLDER, NULL);
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
	remove_proc_entry(PROC_FILE, proc_parent);
	remove_proc_entry(PROC_FOLDER, NULL);

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
			pr_info("kmalloc: %s for buffer", dev->buffer_size);
			dev->buffer = kmalloc(dev->buffer_size, GFP_KERNEL);
			if (dev->buffer == NULL) {
				up(&dev->sync);
				return -ENOMEM;
			}
		}
		dev->sum_write++;
		dev->writing++;
	}

	// check if in read mode
	if (filep->f_mode & FMODE_READ) {
		// noone opened for writing => no buffer => error
		if(dev->buffer == NULL) {
			up(&dev->sync);
			return -ENOMEM;
		}
		dev->sum_read++;
	}

	dev->sum_open++;
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

	dev->sum_release++;
	// release lock
	up(&dev->sync);
	pr_info("mydev_release()=0\n");
	return 0; // for success
}

// return errno negative => return -ENOMEM;
static ssize_t mydev_read(struct file *filep, const char __user *buff, size_t count, loff_t *offset) {
	struct my_cdev *dev;
	int allowed_count;
	int request_count = count;
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

	// are all bytes delivered?
	if(count < request_count) {
		pr_info("mydev_read(): missing data in buffer %ld of %ld copied.\n",count,request_count);
		// is file open with O_NDELAY and do we have someone writing?
		if (!(filep->f_flags & O_NDELAY) && (dev->writing > 0)) {
			size_t bytes_left = request_count - count; // how much bytes are left to copy
			struct timespec ts; // time struct for meassurements
			struct tm tm_out; // used for time displaying
			pr_info("mydev_read(): waiting for data.\n");

			// wait for additional data
			up(&dev->sync); // unlock semaphore for other processes to dadd data
			ts = current_kernel_time(); // get current kernel time
			time_to_tm(ts.tv_sec, 0, &tm_out); // copy data to tm for display
			pr_info("mydev_read(): start waiting for data @%02d:%02d:%02d.%09ld\n",
				tm_out.tm_hour,
				tm_out.tm_min,
				tm_out.tm_sec,
				ts.tv_nsec
			);

			if (wait_event_interruptible(dev->wq_read, ((dev->current_length >= request_count) || (dev->current_length == dev->buffer_size)))) {
				pr_info("mydev_read(): waiting interupted.\n");
				return -ERESTARTSYS;
			}

			ts = current_kernel_time(); // get current kernel time
			time_to_tm(ts.tv_sec, 0, &tm_out); // copy data to tm for display
			pr_info("mydev_read(): end waiting for data @%02d:%02d:%02d.%09ld\n",
				tm_out.tm_hour,
				tm_out.tm_min,
				tm_out.tm_sec,
				ts.tv_nsec
			);

			// lock semaphore again
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}

			allowed_count = dev->current_length - *offset;
			if(allowed_count < bytes_left) {
				bytes_left = allowed_count;
			}

			// copy bytes we waited for
			bytes_left -= copy_to_user(buff+count, dev->buffer + *offset+count, bytes_left);

			// add up total count with additional bytes copied
			count += bytes_left;
		} else {
			pr_info("mydev_read(): no waiting required.\n");
		}
	}

	*offset += count;


	dev->sum_read++;
	// release lock
    up(&dev->sync);
    pr_info("mydev_read()=%d\n",count);
	return count;
}

static ssize_t mydev_write(struct file *filep, const char __user *buff,
		size_t count, loff_t *offset) {
	struct my_cdev *dev;
	int allowed_count = 0;
	int request_count = count;
	pr_info("mydev_write()\n");

	dev = filep->private_data;

	// reset offset after clear operation
	if (*offset > dev->current_length) {
		*offset = dev->current_length;
	}

	// get lock
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	// ho much can w write into buffer
	allowed_count = BUFFER_SIZE - *offset;
	if (allowed_count < count) {
		count = allowed_count;
	}

	// remove successfully copied byte count from overall count
	count -= copy_from_user(dev->buffer + *offset, buff, count);
	*offset += count;

	// adjust current length
	if (dev->current_length < *offset) {
		dev->current_length = *offset;
	}

	// if there was not enough space to write we wait for space
	if ((count < request_count) && !(filep->f_flags & O_NDELAY)) {
		size_t bytes_left = request_count - count; // what is left to do
		struct timespec ts; // time struct for meassurements
		struct tm tm_out; // used for time displaying

		pr_info("mydev_write(): Buffer full written %ld of %ld bytes.\n",count,request_count);

		up(&dev->sync); // release lock
		ts = current_kernel_time(); // get current kernel time
		time_to_tm(ts.tv_sec, 0, &tm_out); // copy data to tm for display
		pr_info("mydev_write(): start waiting for buffer @%02d:%02d:%02d.%09ld\n",
			tm_out.tm_hour,
			tm_out.tm_min,
			tm_out.tm_sec,
			ts.tv_nsec
		);

		if (wait_event_interruptible(dev->wq_write, (*offset > dev->current_length))) {
			pr_info("mydev_write(): waiting interupted.\n");
			return -ERESTARTSYS;
		}

		ts = current_kernel_time(); // get current kernel time
		time_to_tm(ts.tv_sec, 0, &tm_out); // copy data to tm for display
		pr_info("mydev_write(): end waiting for buffer @%02d:%02d:%02d.%09ld\n",
			tm_out.tm_hour,
			tm_out.tm_min,
			tm_out.tm_sec,
			ts.tv_nsec
		);

		// lock semaphore again
		if (down_interruptible(&dev->sync)) {
			return -ERESTARTSYS;
		}

		// do we have enough room now?
		if(dev->buffer_size < bytes_left) {
			bytes_left = dev->buffer_size;
		}

		// copy data to user and update offset
		bytes_left -= copy_from_user(dev->buffer, buff+count, bytes_left);
		*offset = bytes_left;

		// update count with additional bytes copied
		count += bytes_left;

		dev->current_length = bytes_left;
	}

	// wake sleeping readers
	wake_up(&dev->wq_read);

	dev->sum_write++;
	// release lock
	up(&dev->sync);
	pr_info("mydev_write()=%d\n",count);
	return count;
}

static long mydev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
	struct my_cdev *dev;
	long retVal = 0;
	int new_buffer_size;
	char *new_buff;
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
				dev->sum_clear++;
				dev->current_length = 0;
				// wake writers
				wake_up(&dev->wq_write);
				break;
			case IOC_NR_SETBUFFERSIZE:
				new_buffer_size = (int) arg;
				pr_info("mydev_ioctl(): new size %d\n", new_buffer_size);
				if (new_buffer_size == dev->buffer_size) {
					// nothing to do size is already set
					break;
				}

				// is the buffer already there?
				if (dev->buffer != NULL) {
					if (dev->buffer_size > new_buffer_size) { // make it smaller
						dev->current_length = 0;
						kfree(dev->buffer);
						dev->buffer = kmalloc(new_buffer_size, GFP_KERNEL);
						if (dev->buffer == NULL) {
							retVal = -ENOMEM;
							break;
						}
					} else { // make it bigger
						new_buff = kmalloc(new_buffer_size, GFP_KERNEL);
						if (dev->buffer == NULL) {
							retVal = -ENOMEM;
							break;
						}
						// copy old data
						memcpy(new_buff, dev->buffer, dev->current_length);
						// free old buffer
						kfree(dev->buffer);
						// assign new buffer
						dev->buffer = new_buff;
					}
				}
				// set new size
				dev->buffer_size = new_buffer_size;
			default:
				;
		}
	}
	// release lock
	up(&dev->sync);
	pr_info("mydev_ioctl()=%ld\n", retVal);
	return retVal;
}

// The proc file interface
static int my_seq_file_open(struct inode *inode, struct file *filep) {
    return seq_open(filep, &my_seq_options); // init seq_file
}

static void *my_seq_file_start(struct seq_file *sf, loff_t *pos) {
    pr_info("my_seq_file_start()\n");
    if (*pos == 0) {
        return my_devs;
    }
    return NULL;
}

static void my_seq_file_stop(struct seq_file *sf, void *it) {
	pr_info("my_seq_file_stop()\n");
}

static void *my_seq_file_next(struct seq_file *sf, void *it, loff_t *pos) {
	pr_info("my_seq_file_next()\n");
    (*pos)++;
    if (*pos >= MINOR_COUNT) {
        return NULL;
    }
    return &my_devs[*pos];
}

static int my_seq_file_show(struct seq_file *sf, void *it) {
    struct my_cdev *dev = (struct my_cdev*) it;
    int index = dev - my_devs;
    pr_info("my_seq_file_show()\n");

    // Accquire semaphore
    if(down_interruptible(&dev->sync)) {
        return -ERESTARTSYS;
    }
    // The output
    seq_printf(sf, "mydev%d: O: %d, R: %d, W: %d, C: %d, Clear: %d, Cur.length: %d, Puffersize: %d\n",
        index,
        dev->sum_open,
        dev->sum_read,
        dev->sum_write,
        dev->sum_release,
        dev->sum_clear,
        dev->current_length,
        dev->buffer_size
    );

    // Release Semaphore
    up(&dev->sync);
    return 0;
}
