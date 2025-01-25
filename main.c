#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include "main.h"
#include "util.h"



// global variables
dev_t devno;
scull_dev *scull_devices[NUM_DEVICES];

// scull_init
// register device numbers (using alloc_chrdev_region or register_chrdev_region)
// initialize character devices (cdev structures and associate file operations)
// create and initialize any necessary data structures or resources
// register the device with cdev_add
// create associated device nodes (using mknod, typically done by user-space scripts)
static int __init scull_init(void) {
    char *name = "scull";
    int result = alloc_chrdev_region(&devno, BASE_MINOR, NUM_DEVICES, name);
    if (result) {
        printk(KERN_WARNING "scull: can't get major %d\n", devno);
        return result;
    }   
    // init NUM_DEVICES of scull_fops starting at BASE_MINOR
    for (size_t i = 0; i < NUM_DEVICES; ++i) {
        // assign new char device to each kernal device
        scull_devices[i] = kmalloc(sizeof(scull_dev), GFP_KERNEL);
        if (!scull_devices[i]) goto fail;
        scull_setup_cdev(scull_devices[i], (int)(i + BASE_MINOR));
    } // for
    printk(KERN_INFO "Successfully allocated device major/minor and matched device");
    return 0;
    fail:
        for (size_t i = 0; i < NUM_DEVICES; ++i) {
            if (scull_devices[i]) {
                cdev_del(&(scull_devices[i]->cdev));
                kfree(scull_devices[i]);
            } // if
        } // for
        unregister_chrdev_region(devno, NUM_DEVICES);
        return -ENOMEM;
} // scull_init()


// module_exit
// unregister character devices (using cdev_del)
// free allocated device numbers (using unregister_chrdev_region)
// clean up and release any resources or data structures
// remove associated device nodes, (typically done by user-space scripts))
static void __exit scull_exit(void) {
    unregister_chrdev_region(devno, NUM_DEVICES);
    // Free memory from device structs
    for (size_t i = 0; i < NUM_DEVICES; ++i) {
        if (scull_devices[i]) {
            cdev_del(&(scull_devices[i]->cdev));
            kfree(scull_devices[i]);
        } // if 
    } // for
    printk(KERN_INFO "Successfully deallocated device major/minor and matched device");
    return;
} // scull_exit()


// scull_open
// check for device specific errors with inode
// initialize device if opened for the first time
// update the f_op pointer if nessessary (in filp)
// allocate any data needed for othe filp->private_data
int scull_open(struct inode *inode, struct file *filp) {
    scull_dev *dev = container_of(inode->i_cdev, scull_dev, cdev);
    filp->private_data = dev;
    // clear the device if write only flag set
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        scull_trim(dev);
    } // if
    return 0;
} // scull_open()

// scull_release
// deallocate anything which open allocated in filp->private_data
// shutdown device on final release
// note that filp->private_data is emptied by OS
// note release is only invoked on the final close
int scull_release(struct inode *inode, struct file *filp) {
    return 0;
} // scull_release()


// scull_setup_cdev
// to be called by the init funciton
// set up a scull device struct for each of the 4 devices
// mknod creates the device
// mkdev is kernal space macro to extract major and minor device number
// cdev_add is a kernal space function to register the device to the driver
// additionally, the function should be called at the very end of module init
void scull_setup_cdev(scull_dev *dev, int index) {
    dev_t devno_sp = MKDEV(MAJOR(devno), index + BASE_MINOR);
    // formal way of dev->cdev.ops = &scull_fops
    cdev_init(&(dev->cdev), &scull_fops);
    // good practice to also set the owner here
    dev->cdev.owner = THIS_MODULE; 

    int err = cdev_add(&(dev->cdev), devno_sp, 1);
    if (err) {
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
    } // err
} // scull_detup_cdev()


// scull_read
// use kmalloc and kfree from slab.h
ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    // TODO
    char *funny_thing_to_print = "Something funny haha\n";
    size_t len = strlen(funny_thing_to_print) + 1;
    if (copy_to_user(buf, funny_thing_to_print, len)) {
        return -EFAULT;
    } // if
    return len;
} // scull_read()


// scull_write
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    // TODO
    return 0;
} // scull_write()

// TODO
//loff_t scull_llseek(struct file * filp, loff_t , int);




