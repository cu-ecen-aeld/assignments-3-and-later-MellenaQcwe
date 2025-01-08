/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Melchior NN"); 
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("AESD character device driver");

struct aesd_dev aesd_device;
struct aesd_circular_buffer cbuf; // Circular  buffer
struct aesd_buffer_entry *cmd; // NULL initialzed
struct aesd_buffer_entry *cmds[AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED] = {}; // Free at teardown
int cmds_cnt;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    // struct aesd_dev *dev; /* device information */
	// dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	// filp->private_data = dev; /* for other methods */

	return 0; /* success */
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle read
     */

    //struct aesd_dev *dev = filp->private_data; 

    if (mutex_lock_interruptible(&aesd_device.lock))
        return -ERESTARTSYS;

    size_t *byte_rtn; /*byte count from offset*/
    byte_rtn = kmalloc(sizeof(size_t), GFP_KERNEL);
    if (!byte_rtn) {
        printk(KERN_ERR "Failed to allocate byte_rtn memory");
        return retval;
    }

    struct aesd_buffer_entry *cmd; // NULL initialzed
    
    cmd = aesd_circular_buffer_find_entry_offset_for_fpos(&cbuf, *f_pos, byte_rtn);
    if (!cmd) {
        PDEBUG("No entry found for offset %lld", *f_pos);
        goto out;
    }

    cmd->size = strlen(cmd->buffptr);
    if (count > cmd->size) {
        count = cmd->size;
    }
    //PDEBUG("Copy to user: %s", cmd->buffptr);
    if (copy_to_user(buf, cmd->buffptr, count) != 0) {
        if (0 < retval < count) {
            printk(KERN_ERR "Failed to copy all bytes to user (remaining: %ld ; total: %ld)", retval, count);
        } else {
            printk(KERN_ERR "Failed to copy to user");
            retval = -EFAULT;
        }
        goto out;
    }
    /*Copy to user returned 0 -> success*/

    *f_pos += count;
    retval = count;

out:
    mutex_unlock(&aesd_device.lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    static int cmd_offset = 0;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */

    //struct aesd_dev *dev = filp->private_data;

    char *tmp_cmd;

    if (!cmd_offset) {
        cmd = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
        if (!cmd) {
            printk(KERN_ERR "Failed to allocate entry memory");
            return retval;
        }
        cmd->buffptr = kmalloc(count, GFP_KERNEL);
        if (!cmd->buffptr) {
            printk(KERN_ERR "Failed to allocate entry-buffer memory");
            kfree(cmd);
            return retval;
        }

        //cmds[cmds_cnt++] = cmd;
    }

    if (mutex_lock_interruptible(&aesd_device.lock))
		return -ERESTARTSYS;
    
    tmp_cmd = cmd->buffptr;
    if (copy_from_user(tmp_cmd + cmd_offset, buf, count) != 0) {
        printk(KERN_ERR "Failed to copy from user (remaining: %ld ; total: %ld)", retval, count);
        retval = -EFAULT;
        goto out;
    }
    /*Copy from user returned 0 -> success*/

    cmd_offset += count;
    if (*(cmd->buffptr + (cmd_offset-1)) == '\n') {
        *(tmp_cmd + cmd_offset) = '\0';
        cmd->size = cmd_offset;
        aesd_circular_buffer_add_entry(&cbuf, cmd);
        // if (aesd_device.cbuf->in_offs) {
        //     PDEBUG("Cmd (size = %ld) at %d is %s", cmd->size, (aesd_device.cbuf->in_offs-1), aesd_device.cbuf->entry[(aesd_device.cbuf->in_offs-1)].buffptr);
        // }
        cmd_offset = 0;
    } 

    retval = count;

out:
    mutex_unlock(&aesd_device.lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

	memset(&aesd_device, 0, sizeof(struct aesd_dev));
    aesd_circular_buffer_init(&cbuf);
    mutex_init(&aesd_device.lock);
    cmds_cnt = 0;

    result = aesd_setup_cdev(&aesd_device);
    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // for (;;) {  
    //     kfree(cmds[cmds_cnt]->buffptr);
    //     kfree(cmds[cmds_cnt]);
    //     if (!cmds_cnt--) break;
    // }
    kfree(cmd->buffptr);
    kfree(cmds);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
