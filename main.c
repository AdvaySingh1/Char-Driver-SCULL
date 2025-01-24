#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>



// init and exit functions
static int __init scull_init(void);
static int __exit scull_exit(void);
loff_t scull_llseek(struct file *, loff_t, int);
ssize_t scull_read(struct file *, char __user *, size_t, loff_t *);
ssize_t scull_write(struct file *, char __user *, size_t, loff_t *);
int scull_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
int scull_open(struct indoe *, struct file *);
int scull_release(struct inode *, struct file *);

// module init and exit
MODULE_INIT(skill_init);
MODULE_EXIT(scull_exit);

// license, versioining (not interopreble with OS versioning)
MODULE_DESCRIPTION("Simple Character Utility for Loading Localities");
MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BST/GPL");

// other helper functions
void scull_setup_cdev(struct scull_dev *, int);

// exported types form utils
// device struct
export struct scull_dev;
export struct scull_qset;

// file_operations struct
// many operations like mmap and poll not defined
struct file_operations scull_fops = {
    .owner = THIS_MODULE, // ensure that module can't be unloaded while cdev's registered to this module
    .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    .ioctl = scull_ioctl,
    .open = scull_open,
    .release = scull_release,
}; // file_operations


// scull_init
// register device numbers (using alloc_chrdev_region or register_chrdev_region)
// initialize character devices (cdev structures and associate file operations)
// create and initialize any necessary data structures or resources
// register the device with cdev_add
// create associated device nodes (using mknod, typically done by user-space scripts)
static int __init scull_init(void) {

} // scull_init()


// module_exit
// unregister character devices (using cdev_del)
// free allocated device numbers (using unregister_chrdev_region)
// clean up and release any resources or data structures
// remove associated device nodes, (typically done by user-space scripts))
static int __exit scull_exit(void) {

} // scull_exit()


// scull_open
// check for device specific errors with inode
// initialize device if opened for the first time
// update the f_op pointer if nessessary (in filp)
// allocate any data needed for othe filp->private_data
int scull_open(struct inode *inode, struct file *filp) {
    struct scull_dev *dev = container_of(inode->c_dev, scull)
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
} // skill_release()


// scull_setup_cdev
// to be called by the init funciton
// set up a scull device struct for each of the 4 devices
// mknod creates the device
// mkdev is kernal space macro to extract major and minor device number
// cdev_add is a kernal space function to register the device to the driver
// additionally, the function should be called at the very end of module init
void scull_setup_cdev(struct scull_dev *dev, int index) {
    dev_t devno = MKDEV(scull_major, scull_minor + index);

    // formal way of dev->cdev.ops = &scull_fops
    cdev_init(&(dev->cdev), &scull_fops);
    // good practice to also set the owner here
    dev->cdev.owner = THIS_MODULE; 

    int err = cdev_add(&(dev->cdev), devno, 1);
    if (err) {
        printl(KERN_NOTICE "Error %d adding scull%d", err, index);
    } // err
} // scull_detup_cdev()

