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
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Abhishek Koppa"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
struct aesd_circular_buffer aesd_write_buffer;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    return 0;
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
    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
     size_t bytes_read = 0;
     size_t read_offset;
     struct aesd_buffer_entry *start_read_buffer = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_write_buffer, *f_pos, &read_offset);
     if(start_read_buffer == NULL)
     {
         return 0;
     }
     int current_index = start_read_buffer - aesd_write_buffer.entry;
     while(count>0)
     {
        size_t bytes_available = start_read_buffer->size - read_offset;
        size_t bytes_to_copy = min(count, bytes_available);
        if (copy_to_user(buf + bytes_read, start_read_buffer->buffptr + read_offset, bytes_to_copy)) 
        {
            return -EFAULT; 
        }
        bytes_read += bytes_to_copy;
        count -= bytes_to_copy;
        *f_pos += bytes_to_copy;
        read_offset += bytes_to_copy;

        if (count == 0) 
        {
            break; 
        }
        
        current_index = (current_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        
        if ((current_index == aesd_write_buffer.in_offs) && (!aesd_write_buffer.full))
        {
            break; // No more data left to read
        }

        start_read_buffer = &aesd_write_buffer.entry[current_index];
        read_offset = 0; 
     }
     return bytes_read;
}
struct temp_buffer 
{
    char *data;
    size_t size;
    size_t capacity;
} temp_buf;


ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
     if (temp_buf.data == NULL) 
     {
        temp_buf.data = kmalloc(count + 50, GFP_KERNEL);
        if (temp_buf.data == NULL)
        return -ENOMEM;
        temp_buf.capacity = count + 50;
        temp_buf.size = 0;
    }
    else if (temp_buf.capacity < temp_buf.size + count) 
    {
    size_t new_capacity = temp_buf.size + count + 50;
    char *new_data = krealloc(temp_buf.data, new_capacity, GFP_KERNEL);
    if (new_data == NULL)
        return -ENOMEM;

    temp_buf.data = new_data;
    temp_buf.capacity = new_capacity;
    }
    
    if (copy_from_user(temp_buf.data + temp_buf.size, buf, count)) 
    {
        //kfree(buffer);
        kfree(buffer_struct);
        return -EFAULT;
    }
    
    temp_buf.size += count; 
    

    if (memchr(temp_buf.data, '\n', temp_buf.size))
    {
    	
        struct aesd_buffer_entry *buffer_struct = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
   	if(buffer_struct == NULL)
   	{
       	    return -ENOMEM;

   	}
   	
   	char *command_data = kmalloc(temp_buf.size, GFP_KERNEL);
    if (command_data == NULL) {
        kfree(buffer_struct);
        return -ENOMEM;
    }

    memcpy(command_data, temp_buf.data, temp_buf.size);
    buffer_struct->buffptr = command_data;
    
    buffer_struct->size  = temp_buf.size;
	
    aesd_circular_buffer_add_entry(aesd_write_buffer, buffer_struct);
    	
    	//Just reset the size
    	
        temp_buf.size = 0;
        
    }
   
    

    //retval = count; // Set retval after successful writes
    return count;
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
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
 
    aesd_circular_buffer_init(&aesd_write_buffer);
    temp_buf.data = NULL;
    temp_buf.size = 0;
    temp_buf.capacity = 0;

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

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
     if(temp_buf.data != NULL) 
     {
        kfree(temp_buf.data);
        temp_buf.data = NULL;
        temp_buf.size = 0;
        temp_buf.capacity = 0;
     }
     
     //Clean the CBFIFO 
     struct aesd_buffer_entry *entryptr;
     int index;
     AESD_CIRCULAR_BUFFER_FOREACH(entryptr, aesd_write_buffer, index) 
     {
        if (entryptr->buffptr) 
        {  
            free(entryptr->buffptr);
            entryptr->buffptr = NULL;
            entryptr->size = 0;  
        }
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
