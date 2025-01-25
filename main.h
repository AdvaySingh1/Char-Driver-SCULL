# pragma once
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include "main.h"
#include "util.h"

#ifndef MAIN_H
#define MAIN_H

// macros
# define BASE_MINOR 0
# define NUM_DEVICES 4

// init and exit functions
static int __init scull_init(void);
static void __exit scull_exit(void);
//loff_t scull_llseek(struct file *, loff_t, int); TODO
ssize_t scull_read(struct file *, char __user *, size_t, loff_t *);
ssize_t scull_write(struct file *, const char __user *, size_t, loff_t *);
//int scull_ioctl(struct inode *, struct file *, unsigned int, unsigned long); replaced after 2.6.36
int scull_open(struct inode *, struct file *);
int scull_release(struct inode *, struct file *);

// module init and exit
module_init(scull_init);
module_exit(scull_exit);

// license, versioining (not interopreble with OS versioning)
MODULE_DESCRIPTION("Simple Character Utility for Loading Localities");
MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BST/GPL");

// other helper functions
void scull_setup_cdev(struct scull_dev *, int);


// file_operations struct
// many operations like mmap and poll not defined
struct file_operations scull_fops = {
    .owner = THIS_MODULE, // ensure that module can't be unloaded while cdev's registered to this module
    .read = scull_read,
    .write = scull_write,
    .open = scull_open,
    .release = scull_release,
    //.llseek = scull_llseek, TODO
    //.ioctl = scull_ioctl, replaced
}; // file_operations

extern dev_t devno;
extern scull_dev scull_devies[NUM_DEVICES];


#endif