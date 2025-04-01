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
#include <linux/mutex.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"
#include <linux/kernel.h> 

#define aesdchar_IO_MAGIC 'a'



int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Abhishek Koppa"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

#define MAX_TEMP_BUFFER_SIZE 1024

struct aesd_dev aesd_device;


int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
     struct aesd_dev *dev;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    PDEBUG("open successful\n");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    filp->private_data = NULL; 
    PDEBUG("release successful\n");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;  // Get device structure
    ssize_t bytes_read = 0;
    size_t read_offset;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    mutex_lock(&dev->aesd_CB_mutex);

    struct aesd_buffer_entry *start_read_buffer = 
        aesd_circular_buffer_find_entry_offset_for_fpos(&dev->aesd_write_buffer, *f_pos, &read_offset);

    if (start_read_buffer == NULL) 
    {
        PDEBUG("No data available (EOF)\n");
        mutex_unlock(&dev->aesd_CB_mutex);
        return 0;  // No data available (EOF)
    }

    // Determine how much data is available in the current buffer
    size_t bytes_available = start_read_buffer->size - read_offset;
    size_t bytes_to_copy = min(count, bytes_available);

    PDEBUG("Copying %zu bytes from buffer\n", bytes_to_copy);

    // Copy data to user space
    if (copy_to_user(buf, start_read_buffer->buffptr + read_offset, bytes_to_copy)) 
    {
        PDEBUG("Error copying to user space\n");
        mutex_unlock(&dev->aesd_CB_mutex);
        return -EFAULT;
    }

    // Update read position
    *f_pos += bytes_to_copy;
    bytes_read = bytes_to_copy;

    PDEBUG("read successful, returning %zd bytes\n", bytes_read);
    mutex_unlock(&dev->aesd_CB_mutex);
    return bytes_read;  // Return number of bytes read (could be partial)
}





ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;  // Get device structure
    ssize_t retval = -ENOMEM;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    mutex_lock(&dev->aesd_CB_mutex);

    if (dev->temp_buf.data == NULL) 
    {
        PDEBUG("Memory allocation failed for temp buffer\n");
        dev->temp_buf.data = kmalloc(count + 50, GFP_KERNEL);
        if (dev->temp_buf.data == NULL) 
        {
            mutex_unlock(&dev->aesd_CB_mutex);
            return -ENOMEM;
        }
        dev->temp_buf.capacity = count + 50;
        dev->temp_buf.size = 0;
    }
    else if (dev->temp_buf.capacity < dev->temp_buf.size + count) 
    {
        size_t new_capacity = dev->temp_buf.size + count + 50;
        
        char *new_data = krealloc(dev->temp_buf.data, new_capacity, GFP_KERNEL);
        if (new_data == NULL) 
        {
            PDEBUG("Reallocation failed\n");
            mutex_unlock(&dev->aesd_CB_mutex);
            return -ENOMEM;
        }

        dev->temp_buf.data = new_data;
        dev->temp_buf.capacity = new_capacity;
    }

    if (copy_from_user(dev->temp_buf.data + dev->temp_buf.size, buf, count)) 
    {
        PDEBUG("Error copying from user space\n");
        dev->temp_buf.size = 0;
        mutex_unlock(&dev->aesd_CB_mutex);
        return -EFAULT;
    }

    dev->temp_buf.size += count;

    if (memchr(dev->temp_buf.data, '\n', dev->temp_buf.size)) 
    {
        struct aesd_buffer_entry buffer_struct;

        char *command_data = kmalloc(dev->temp_buf.size, GFP_KERNEL);
        if (command_data == NULL) 
        {
            PDEBUG("Memory allocation failed for command_data\n");
            mutex_unlock(&dev->aesd_CB_mutex);
            return -ENOMEM;
        }

        memcpy(command_data, dev->temp_buf.data, dev->temp_buf.size);
        buffer_struct.buffptr = command_data;
        buffer_struct.size = dev->temp_buf.size;

        const char *old_entry = aesd_circular_buffer_add_entry(&dev->aesd_write_buffer, &buffer_struct);

        // Free the old entry if necessary
        if (old_entry) 
        {
            PDEBUG("Freeing old buffer entry\n");
            kfree((void*)old_entry);
        }

        dev->temp_buf.size = 0;
    }
    *f_pos += count;
	
    mutex_unlock(&dev->aesd_CB_mutex);
    PDEBUG("write successful, wrote %zu bytes\n", count);
    return count;
}


static loff_t aesd_llseek(struct file *file, loff_t offset, int whence)
{
    struct aesd_dev *dev = file->private_data;
    loff_t total_size;
    
    mutex_lock(&dev->aesd_CB_mutex);
    total_size = aesd_circular_buffer_total_size(&dev->aesd_write_buffer);
    mutex_unlock(&dev->aesd_CB_mutex);
    
    return fixed_size_llseek(file, offset, whence, total_size);
}

static long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    struct aesd_dev *dev = filp->private_data;
    
    if(cmd == AESDCHAR_IOCSEEKTO)
    {
        if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        {
            return -EFAULT;   
        }
        
        
        mutex_lock(&dev->aesd_CB_mutex);
        if(seekto.write_cmd >= aesd_circular_buffer_size(&dev->aesd_write_buffer))
        {
            return -EINVAL;
        }
        
        
        int actual_index = (dev->aesd_write_buffer.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        if(seekto.write_cmd_offset >= dev->aesd_write_buffer.entry[actual_index].size)
        {
            return -EINVAL;
        }
        
        loff_t new_f_pos = 0;
        for(int i = 0; i < seekto.write_cmd; i++)
        {
            new_f_pos += dev->aesd_write_buffer.entry[(dev->aesd_write_buffer.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;  //Traverse until the required entry, appending their sizes
        }
        
        mutex_unlock(&dev->aesd_CB_mutex);
        new_f_pos += seekto.write_cmd_offset;		//Include the offset given as well to the new f_pos, as that is was included
        
        filp->f_pos = new_f_pos;
        
        pr_info("IOCTL SEEK: cmd=%u, offset=%u, f_pos=%lld\n", seekto.write_cmd, seekto.write_cmd_offset, new_f_pos);
        return 0;
    }
    pr_info("Cmd used: %ld", cmd);
    return -ENOTTY;    //wrong command chosen.
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .llseek =   aesd_llseek,
    .unlocked_ioctl 	= 	aesd_unlocked_ioctl,
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
        PDEBUG("Error adding aesd cdev\n");
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    PDEBUG("Initializing AESD Char Module\n");
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        PDEBUG("Can't get major number\n");
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
     mutex_init(&aesd_device.aesd_CB_mutex);
     aesd_circular_buffer_init(&aesd_device.aesd_write_buffer);
     aesd_device.temp_buf.data = NULL;
     aesd_device.temp_buf.size = 0;
     aesd_device.temp_buf.capacity = 0;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    PDEBUG("AESD Char Module Initialized\n");
    return result;

}

void aesd_cleanup_module(void)
{
    PDEBUG("Cleaning up AESD Char Module\n");

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    if (aesd_device.temp_buf.data != NULL) 
    {
        kfree(aesd_device.temp_buf.data);
        aesd_device.temp_buf.data = NULL;
        aesd_device.temp_buf.size = 0;
        aesd_device.temp_buf.capacity = 0;
    }
     
     //Clean the CBFIFO 
    struct aesd_buffer_entry *entryptr;
    int index;
    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.aesd_write_buffer, index) 
    {
        if (entryptr->buffptr) 
        {  
            kfree(entryptr->buffptr);  
            entryptr->buffptr = NULL;
            entryptr->size = 0;  
        }
    }

    unregister_chrdev_region(devno, 1);
    PDEBUG("AESD Char Module Cleaned Up\n");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
