#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>

#ifndef UTIL_H
#define UTIL_H

# define SCULL_QUANTUM_SIZE 4000;
# define SCULL_QSET_SIZE 1000;


typedef struct scull_qset {
    void **data;
    struct scull_qset *next;
} scull_qset; // struct scull_qset


typedef struct scull_dev {
    struct scull_qset *data; /* pointer to first quantum set */
    int quantum;             /* current quantum set */
    int qset;                /* number of quantum sets */
    unsigned long size;      /* amount of data stored here */
    unsigned int access_key; /* later used by sculluid and scullpriv */
    struct semaphore sem;    /* mutual exclusion semaphore */
    struct cdev cdev;        /* char device structure */
} scull_dev; // struct scull_dev


// trim functionality to clear the device's memory
// kernal mode disallows page faults, hence kfree
int scull_trim(struct scull_dev *);


# endif